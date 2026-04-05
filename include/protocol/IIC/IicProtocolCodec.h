#ifndef IIC_PROTOCOL_CODEC_H
#define IIC_PROTOCOL_CODEC_H

#include <stddef.h>
#include <stdint.h>

namespace esp32s3
{
namespace iic
{
static const uint8_t kProtocolVersion = 1;
static const uint8_t kMaxPayloadLen = 48;
static const uint8_t kMinFrameLen = 5;
static const uint8_t kMaxFrameLen = kMinFrameLen + kMaxPayloadLen;

enum MessageType : uint8_t
{
  MSG_PING = 0x01,
  MSG_GET_STATUS = 0x02,
  MSG_TRIGGER_CAPTURE = 0x03,
  MSG_REBOOT_NODE = 0x04,
  MSG_GET_LAST_RESULT = 0x05,
  MSG_SET_WIFI_CONFIG = 0x06,
  MSG_SET_WS_CONFIG = 0x07,
  MSG_PROVISION_FRAME = 0x08,
};

enum ErrorCode : uint8_t
{
  ERR_OK = 0x00,
  ERR_BAD_CRC = 0x01,
  ERR_BAD_LEN = 0x02,
  ERR_UNSUPPORTED_CMD = 0x03,
  ERR_BUSY = 0x04,
  ERR_INTERNAL_ERROR = 0x05,
};

struct FrameView
{
  uint8_t version;
  uint8_t msgType;
  uint8_t seq;
  uint8_t payloadLen;
  uint8_t payload[kMaxPayloadLen];
  uint8_t crc8;
};

class IicProtocolCodec
{
 public:
  static uint8_t crc8(const uint8_t* data, size_t len);

  static bool encode(uint8_t msgType, uint8_t seq, const uint8_t* payload,
                     uint8_t payloadLen, uint8_t* outBuf, size_t outBufSize,
                     size_t* outLen);

  static bool decode(const uint8_t* data, size_t len, FrameView* outFrame,
                     ErrorCode* outErr = nullptr);
};
}  // namespace iic
}  // namespace esp32s3

#endif