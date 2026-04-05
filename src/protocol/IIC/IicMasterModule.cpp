#include "protocol/IIC/IicMasterModule.h"

#include <Arduino.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>

#include "LogSwitch.h"
#include "protocol/IIC/IicProtocolCodec.h"

namespace esp32s3
{
namespace
{
#ifndef IIC_MASTER_SDA_PIN
#define IIC_MASTER_SDA_PIN 18
#endif

#ifndef IIC_MASTER_SCL_PIN
#define IIC_MASTER_SCL_PIN 17
#endif

#ifndef IIC_MASTER_FREQ_HZ
#define IIC_MASTER_FREQ_HZ 100000
#endif

#ifndef IIC_MASTER_POLL_MS
#define IIC_MASTER_POLL_MS 200
#endif

#ifndef IIC_MASTER_RETRY_COUNT
#define IIC_MASTER_RETRY_COUNT 2
#endif

#ifndef IIC_MASTER_TASK_STACK_WORDS
#define IIC_MASTER_TASK_STACK_WORDS 4096
#endif

#ifndef IIC_MASTER_TASK_PRIORITY
#define IIC_MASTER_TASK_PRIORITY 1
#endif

static const uint8_t kNodeAddressA = 0x24;
static const uint8_t kNodeAddressB = 0x25;
static const uint8_t kNodeCount = 2;

struct NodeRuntime
{
  iic::NodeStatus status;
};

TwoWire s_iicBus(1);
SemaphoreHandle_t s_busMutex = nullptr;
TaskHandle_t s_pollTaskHandle = nullptr;
bool s_initialized = false;
uint8_t s_nextSeq = 1;

NodeRuntime s_nodes[kNodeCount] = {
    {{kNodeAddressA, false, 0, iic::ERR_OK, 0, 0}},
    {{kNodeAddressB, false, 0, iic::ERR_OK, 0, 0}},
};

static bool findNodeRuntime(uint8_t address, NodeRuntime** outRuntime)
{
  if (outRuntime != nullptr)
  {
    *outRuntime = nullptr;
  }

  for (uint8_t i = 0; i < kNodeCount; ++i)
  {
    if (s_nodes[i].status.address == address)
    {
      if (outRuntime != nullptr)
      {
        *outRuntime = &s_nodes[i];
      }
      return true;
    }
  }
  return false;
}

static uint8_t allocSeq()
{
  uint8_t seq = s_nextSeq;
  ++s_nextSeq;
  if (s_nextSeq == 0)
  {
    s_nextSeq = 1;
  }
  return seq;
}

static bool writePacket(uint8_t address, const uint8_t* data, size_t len)
{
  if (data == nullptr || len == 0)
  {
    return false;
  }

  s_iicBus.beginTransmission(address);
  size_t written = s_iicBus.write(data, len);
  uint8_t err = s_iicBus.endTransmission(true);

  if (written != len)
  {
    LOG_PRINTF(LOG_IIC,
               "[IIC] TX short write addr=0x%02X expect=%u actual=%u\n",
               address, (unsigned int)len, (unsigned int)written);
    return false;
  }

  if (err != 0)
  {
    LOG_PRINTF(LOG_IIC, "[IIC] TX failed addr=0x%02X err=%u\n", address, err);
    return false;
  }

  return true;
}

static bool readPacket(uint8_t address, uint8_t* outBuf, size_t outBufSize,
                       size_t* outLen)
{
  if (outLen != nullptr)
  {
    *outLen = 0;
  }

  if (outBuf == nullptr || outBufSize < iic::kMinFrameLen)
  {
    return false;
  }

  size_t got = s_iicBus.requestFrom((int)address, (int)outBufSize, (int)true);
  if (got < iic::kMinFrameLen)
  {
    LOG_PRINTF(LOG_IIC, "[IIC] RX too short addr=0x%02X len=%u\n", address,
               (unsigned int)got);
    while (s_iicBus.available() > 0)
    {
      (void)s_iicBus.read();
    }
    return false;
  }

  size_t index = 0;
  while (s_iicBus.available() > 0 && index < got && index < outBufSize)
  {
    outBuf[index++] = (uint8_t)s_iicBus.read();
  }

  while (s_iicBus.available() > 0)
  {
    (void)s_iicBus.read();
  }

  if (outLen != nullptr)
  {
    *outLen = index;
  }
  return true;
}

static bool transceive(uint8_t address, uint8_t msgType, const uint8_t* payload,
                       uint8_t payloadLen, iic::FrameView* outResponse)
{
  uint8_t txBuf[iic::kMaxFrameLen] = {0};
  size_t txLen = 0;
  const uint8_t seq = allocSeq();

  if (!iic::IicProtocolCodec::encode(msgType, seq, payload, payloadLen, txBuf,
                                     sizeof(txBuf), &txLen))
  {
    return false;
  }

  for (int attempt = 0; attempt <= IIC_MASTER_RETRY_COUNT; ++attempt)
  {
    if (xSemaphoreTake(s_busMutex, pdMS_TO_TICKS(50)) != pdTRUE)
    {
      continue;
    }

    bool txOk = writePacket(address, txBuf, txLen);
    delay(2);

    uint8_t rxBuf[iic::kMaxFrameLen] = {0};
    size_t rxLen = 0;
    bool rxOk = txOk && readPacket(address, rxBuf, sizeof(rxBuf), &rxLen);
    xSemaphoreGive(s_busMutex);

    if (!rxOk)
    {
      continue;
    }

    iic::ErrorCode decodeErr = iic::ERR_OK;
    iic::FrameView frame = {};
    if (!iic::IicProtocolCodec::decode(rxBuf, rxLen, &frame, &decodeErr))
    {
      LOG_PRINTF(LOG_IIC,
                 "[IIC] decode failed addr=0x%02X err=%u len=%u attempt=%d\n",
                 address, decodeErr, (unsigned int)rxLen, attempt);
      continue;
    }

    if (frame.seq != seq)
    {
      LOG_PRINTF(LOG_IIC,
                 "[IIC] seq mismatch addr=0x%02X tx=%u rx=%u attempt=%d\n",
                 address, seq, frame.seq, attempt);
      continue;
    }

    if (outResponse != nullptr)
    {
      *outResponse = frame;
    }
    return true;
  }

  return false;
}

static void updateNodeOnline(uint8_t address, bool online)
{
  NodeRuntime* node = nullptr;
  if (!findNodeRuntime(address, &node) || node == nullptr)
  {
    return;
  }

  if (node->status.online != online)
  {
    LOG_PRINTF(LOG_IIC, "[IIC] node 0x%02X %s\n", address,
               online ? "ONLINE" : "OFFLINE");
  }

  node->status.online = online;
  if (online)
  {
    node->status.lastSeenMs = millis();
  }
}

static bool parseStatusPayload(NodeRuntime* node, const iic::FrameView& frame)
{
  if (node == nullptr)
  {
    return false;
  }

  if (frame.payloadLen < 5)
  {
    node->status.lastError = iic::ERR_BAD_LEN;
    return false;
  }

  node->status.lastError = frame.payload[1];
  node->status.lastSeq = frame.payload[2];
  node->status.lastFrameCounter = (uint16_t)((uint16_t)frame.payload[3] |
                                             ((uint16_t)frame.payload[4] << 8));
  return true;
}

static bool requestStatus(uint8_t address)
{
  iic::FrameView response = {};
  if (!transceive(address, iic::MSG_GET_STATUS, nullptr, 0, &response))
  {
    updateNodeOnline(address, false);
    return false;
  }

  NodeRuntime* node = nullptr;
  if (!findNodeRuntime(address, &node) || node == nullptr)
  {
    return false;
  }

  if (!parseStatusPayload(node, response))
  {
    updateNodeOnline(address, false);
    return false;
  }

  updateNodeOnline(address, true);
  return true;
}

static void pollTask(void* pvParameters)
{
  LOG_PRINTF(LOG_IIC,
             "[IIC] poll task started, nodes=[0x%02X,0x%02X], period=%u ms\n",
             kNodeAddressA, kNodeAddressB, (unsigned int)IIC_MASTER_POLL_MS);

  while (true)
  {
    (void)requestStatus(kNodeAddressA);
    (void)requestStatus(kNodeAddressB);
    vTaskDelay(pdMS_TO_TICKS(IIC_MASTER_POLL_MS));
  }
}

static bool sendSimpleCommand(uint8_t address, uint8_t msgType)
{
  iic::FrameView response = {};
  if (!transceive(address, msgType, nullptr, 0, &response))
  {
    return false;
  }

  updateNodeOnline(address, true);
  NodeRuntime* node = nullptr;
  if (!findNodeRuntime(address, &node) || node == nullptr)
  {
    return false;
  }

  if (response.payloadLen > 0)
  {
    node->status.lastError = response.payload[0];
  }

  return node->status.lastError == iic::ERR_OK;
}

static bool sendPayloadCommand(uint8_t address, uint8_t msgType,
                               const uint8_t* payload, uint8_t payloadLen)
{
  iic::FrameView response = {};
  if (!transceive(address, msgType, payload, payloadLen, &response))
  {
    return false;
  }

  updateNodeOnline(address, true);
  NodeRuntime* node = nullptr;
  if (!findNodeRuntime(address, &node) || node == nullptr)
  {
    return false;
  }

  if (response.payloadLen > 0)
  {
    node->status.lastError = response.payload[0];
  }

  return node->status.lastError == iic::ERR_OK;
}

static bool startsWith(const char* text, const char* prefix)
{
  if (text == nullptr || prefix == nullptr)
  {
    return false;
  }

  size_t prefixLen = strlen(prefix);
  return strncmp(text, prefix, prefixLen) == 0;
}

static bool sendProvisionFrame(uint8_t address, uint8_t marker,
                               const uint8_t* data, uint8_t dataLen)
{
  if (dataLen > (iic::kMaxPayloadLen - 1))
  {
    return false;
  }

  uint8_t payload[iic::kMaxPayloadLen] = {0};
  payload[0] = marker;
  for (uint8_t i = 0; i < dataLen; ++i)
  {
    payload[1 + i] = data[i];
  }

  return sendPayloadCommand(address, iic::MSG_PROVISION_FRAME, payload,
                            (uint8_t)(dataLen + 1));
}

static bool initImpl()
{
  if (s_initialized)
  {
    return true;
  }

  s_busMutex = xSemaphoreCreateMutex();
  if (s_busMutex == nullptr)
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] mutex create failed");
    return false;
  }

  bool beginOk = s_iicBus.begin(IIC_MASTER_SDA_PIN, IIC_MASTER_SCL_PIN,
                                (uint32_t)IIC_MASTER_FREQ_HZ);
  if (!beginOk)
  {
    LOG_PRINTF(LOG_IIC, "[IIC] bus begin failed sda=%u scl=%u\n",
               IIC_MASTER_SDA_PIN, IIC_MASTER_SCL_PIN);
    return false;
  }

  LOG_PRINTF(LOG_IIC, "[IIC] bus init sda=%u scl=%u freq=%u\n",
             IIC_MASTER_SDA_PIN, IIC_MASTER_SCL_PIN,
             (unsigned int)IIC_MASTER_FREQ_HZ);

  s_initialized = true;
  return true;
}

static bool startTasksImpl()
{
  if (!s_initialized)
  {
    return false;
  }

  if (s_pollTaskHandle != nullptr)
  {
    return true;
  }

  BaseType_t ok = xTaskCreatePinnedToCore(
      pollTask, "IIC_Poll_Task", IIC_MASTER_TASK_STACK_WORDS, nullptr,
      IIC_MASTER_TASK_PRIORITY, &s_pollTaskHandle, 0);

  if (ok != pdPASS)
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] poll task create failed");
    return false;
  }

  return true;
}
}  // namespace

bool IicMasterModule::init() { return initImpl(); }

bool IicMasterModule::startTasks() { return startTasksImpl(); }

bool IicMasterModule::pingNode(uint8_t address)
{
  return sendSimpleCommand(address, iic::MSG_PING);
}

bool IicMasterModule::triggerCapture(uint8_t address)
{
  return sendSimpleCommand(address, iic::MSG_TRIGGER_CAPTURE);
}

bool IicMasterModule::rebootNode(uint8_t address)
{
  return sendSimpleCommand(address, iic::MSG_REBOOT_NODE);
}

bool IicMasterModule::setWifiConfig(uint8_t address, const char* ssid,
                                    const char* password)
{
  if (ssid == nullptr || password == nullptr)
  {
    return false;
  }

  size_t ssidLen = strlen(ssid);
  size_t pwdLen = strlen(password);
  if (ssidLen > 23 || pwdLen > 23)
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] wifi config too long, max=23 bytes each");
    return false;
  }

  uint8_t payload[iic::kMaxPayloadLen] = {0};
  payload[0] = (uint8_t)ssidLen;
  payload[1] = (uint8_t)pwdLen;

  for (size_t i = 0; i < ssidLen; ++i)
  {
    payload[2 + i] = (uint8_t)ssid[i];
  }
  for (size_t i = 0; i < pwdLen; ++i)
  {
    payload[2 + ssidLen + i] = (uint8_t)password[i];
  }

  uint8_t payloadLen = (uint8_t)(2 + ssidLen + pwdLen);
  return sendPayloadCommand(address, iic::MSG_SET_WIFI_CONFIG, payload,
                            payloadLen);
}

bool IicMasterModule::setWebSocketConfig(uint8_t address, const char* host,
                                         uint16_t port, const char* path,
                                         bool useTls)
{
  if (host == nullptr || path == nullptr)
  {
    return false;
  }

  size_t hostLen = strlen(host);
  size_t pathLen = strlen(path);
  if (hostLen > 20 || pathLen > 23)
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] ws config too long, host<=20 path<=23");
    return false;
  }

  uint8_t payload[iic::kMaxPayloadLen] = {0};
  payload[0] = (uint8_t)hostLen;
  payload[1] = (uint8_t)pathLen;
  payload[2] = (uint8_t)(port & 0xFF);
  payload[3] = (uint8_t)((port >> 8) & 0xFF);
  payload[4] = useTls ? 1 : 0;

  for (size_t i = 0; i < hostLen; ++i)
  {
    payload[5 + i] = (uint8_t)host[i];
  }
  for (size_t i = 0; i < pathLen; ++i)
  {
    payload[5 + hostLen + i] = (uint8_t)path[i];
  }

  uint8_t payloadLen = (uint8_t)(5 + hostLen + pathLen);
  return sendPayloadCommand(address, iic::MSG_SET_WS_CONFIG, payload,
                            payloadLen);
}

bool IicMasterModule::sendProvisionConfig(uint8_t address, const char* ssid,
                                          const char* wsUrl,
                                          const char* password, int userId,
                                          int cameraId)
{
  if (ssid == nullptr || wsUrl == nullptr)
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] provision invalid: ssid/ws_url is null");
    return false;
  }

  if (ssid[0] == '\0')
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] provision invalid: ssid is empty");
    return false;
  }

  if (!startsWith(wsUrl, "ws://"))
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] provision invalid: ws_url must start ws://");
    return false;
  }

  const char* safePassword = (password == nullptr) ? "" : password;

  char json[220] = {0};
  int written =
      snprintf(json, sizeof(json),
               "{\"type\":\"provision\",\"ssid\":\"%s\",\"ws_url\":\"%s\","
               "\"password\":\"%s\",\"user_id\":%d,\"camera_id\":%d}",
               ssid, wsUrl, safePassword, userId, cameraId);

  if (written <= 0 || written >= (int)sizeof(json))
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] provision json too long");
    return false;
  }

  const uint8_t* jsonBytes = (const uint8_t*)json;
  uint16_t jsonLen = (uint16_t)written;

  if (!sendProvisionFrame(address, 0x01, nullptr, 0))
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] provision START frame failed");
    return false;
  }

  const uint8_t chunkSize = iic::kMaxPayloadLen - 1;
  uint16_t offset = 0;
  while (offset < jsonLen)
  {
    uint16_t remain = (uint16_t)(jsonLen - offset);
    uint8_t take = (uint8_t)((remain > chunkSize) ? chunkSize : remain);
    if (!sendProvisionFrame(address, 0x02, &jsonBytes[offset], take))
    {
      LOG_PRINTF(LOG_IIC, "[IIC] provision CHUNK frame failed offset=%u\n",
                 (unsigned int)offset);
      return false;
    }
    offset = (uint16_t)(offset + take);
  }

  if (!sendProvisionFrame(address, 0x03, nullptr, 0))
  {
    LOG_PRINTLN(LOG_IIC, "[IIC] provision END frame failed");
    return false;
  }

  return true;
}

bool IicMasterModule::getNodeStatus(uint8_t address, iic::NodeStatus* outStatus)
{
  if (outStatus == nullptr)
  {
    return false;
  }

  NodeRuntime* node = nullptr;
  if (!findNodeRuntime(address, &node) || node == nullptr)
  {
    return false;
  }

  *outStatus = node->status;
  return true;
}
}  // namespace esp32s3