#include "WIFI/WifiModule.h"

#include <WiFi.h>

#include "LogSwitch.h"
#include "WIFI/WifiCredentialStore.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace esp32s3
{
struct WiFiConfig
{
  bool clearOnly;
  char ssid[64];
  char password[64];
};

QueueHandle_t s_wifiConfigQueue = NULL;
TaskHandle_t s_wifiTaskHandle = NULL;
WifiNotifyCallback s_notify = nullptr;
bool s_wifiConnected = false;
WifiCredentialStore s_store;

void sendNotify(const String& message)
{
  if (s_notify != nullptr)
  {
    s_notify(message);
  }
}

void wifiTask(void* pvParameters)
{
  WiFiConfig wifiConfig;
  String savedSsid;
  String savedPassword;

  LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 独立WiFi模块任务启动");

  WiFi.mode(WIFI_STA);
  if (s_store.load(savedSsid, savedPassword))
  {
    LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 检测到已保存凭据,自动连接...");
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

    int bootAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && bootAttempts < 30)
    {
      vTaskDelay(pdMS_TO_TICKS(500));
      bootAttempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      s_wifiConnected = true;
      sendNotify("PROV:OK:IP=" + WiFi.localIP().toString());
      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 自动连接成功");
    }
    else
    {
      s_wifiConnected = false;
      sendNotify("PROV:ERR:TIMEOUT");
      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 自动连接失败,等待蓝牙配网");
    }
  }
  else
  {
    sendNotify("PROV:STATE:IDLE");
    LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 无保存凭据,等待蓝牙配网");
  }

  while (true)
  {
    if (xQueueReceive(s_wifiConfigQueue, &wifiConfig, 0) == pdPASS)
    {
      if (wifiConfig.clearOnly)
      {
        s_store.clear();
        s_wifiConnected = false;
        WiFi.disconnect(false, true);
        sendNotify("PROV:STATE:CLEARED");
        LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 已清除保存凭据");
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }

      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 收到WiFi配置,开始连接...");
      LOG_PRINTF(LOG_WIFI, "[WiFi Task] SSID: %s\n", wifiConfig.ssid);
      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] Password: ******");

      WiFi.disconnect(false, false);
      vTaskDelay(pdMS_TO_TICKS(200));
      WiFi.begin(wifiConfig.ssid, wifiConfig.password);

      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 正在连接WiFi...");
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 30)
      {
        vTaskDelay(pdMS_TO_TICKS(500));
        LOG_PRINT(LOG_WIFI, ".");
        attempts++;
      }
      LOG_PRINTLN(LOG_WIFI, "");

      if (WiFi.status() == WL_CONNECTED)
      {
        s_wifiConnected = true;
        LOG_PRINTLN(LOG_WIFI, "[WiFi Task] !!!!! WiFi连接成功 !!!!!");
        LOG_PRINT(LOG_WIFI, "[WiFi Task] IP地址: ");
        LOG_PRINTLN(LOG_WIFI, WiFi.localIP());

        bool savedOk = s_store.save(wifiConfig.ssid, wifiConfig.password);
        sendNotify("WIFI_CONNECTED:" + WiFi.localIP().toString());
        sendNotify("PROV:OK:IP=" + WiFi.localIP().toString());
        if (!savedOk)
        {
          sendNotify("PROV:ERR:SAVE_FAILED");
        }
      }
      else
      {
        s_wifiConnected = false;
        LOG_PRINTLN(LOG_WIFI, "[WiFi Task] WiFi连接失败!");
        sendNotify("WIFI_FAILED");
        sendNotify("PROV:ERR:TIMEOUT");
      }
    }

    if (s_wifiConnected && WiFi.status() != WL_CONNECTED)
    {
      s_wifiConnected = false;
      LOG_PRINTLN(LOG_WIFI, "[WiFi Task] WiFi已断开连接");
      sendNotify("WIFI_DISCONNECTED");
      if (s_store.load(savedSsid, savedPassword))
      {
        sendNotify("PROV:STATE:RECONNECTING");
        WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
}  // namespace esp32s3

bool esp32s3::WifiModule::init(WifiNotifyCallback notifyCb)
{
  esp32s3::s_notify = notifyCb;
  if (esp32s3::s_wifiConfigQueue == NULL)
  {
    esp32s3::s_wifiConfigQueue = xQueueCreate(2, sizeof(esp32s3::WiFiConfig));
  }

  if (esp32s3::s_wifiConfigQueue == NULL)
  {
    LOG_PRINTLN(LOG_WIFI, "[WiFi Task] 错误: WiFi配置队列创建失败");
    return false;
  }

  return true;
}

bool esp32s3::WifiModule::startTask(UBaseType_t priority, BaseType_t core,
                                    uint32_t stackWords)
{
  if (esp32s3::s_wifiTaskHandle != NULL)
  {
    return true;
  }

  BaseType_t ok =
      xTaskCreatePinnedToCore(esp32s3::wifiTask, "WiFi_Task", stackWords, NULL,
                              priority, &esp32s3::s_wifiTaskHandle, core);
  return ok == pdPASS;
}

bool esp32s3::WifiModule::handleBleCommand(const String& data)
{
  if (esp32s3::s_wifiConfigQueue == NULL)
  {
    return false;
  }

  if (data.startsWith("WIFI:"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: WiFi配网");
    int firstColon = data.indexOf(':', 5);
    if (firstColon > 0)
    {
      String ssid = data.substring(5, firstColon);
      String password = data.substring(firstColon + 1);

      LOG_PRINT(LOG_BLE, "[BLE] WiFi SSID: ");
      LOG_PRINTLN(LOG_BLE, ssid);
      LOG_PRINTLN(LOG_BLE, "[BLE] WiFi Password: ******");

      esp32s3::WiFiConfig wifiConfig;
      wifiConfig.clearOnly = false;
      ssid.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
      password.toCharArray(wifiConfig.password, sizeof(wifiConfig.password));
      xQueueSend(esp32s3::s_wifiConfigQueue, &wifiConfig, 0);
      return true;
    }

    LOG_PRINTLN(LOG_BLE, "[BLE] 错误: WiFi配网格式不正确,应为 WIFI:账号:密码");
    return true;
  }

  if (data.startsWith("PROV:SET:"))
  {
    LOG_PRINTLN(LOG_BLE, "[BLE] 命令: 新协议配网");

    int ssidPos = data.indexOf("SSID=");
    int pwdPos = data.indexOf(";PWD=");
    if (ssidPos >= 0 && pwdPos > ssidPos)
    {
      String ssid = data.substring(ssidPos + 5, pwdPos);
      String password = data.substring(pwdPos + 5);

      esp32s3::WiFiConfig wifiConfig;
      wifiConfig.clearOnly = false;
      ssid.toCharArray(wifiConfig.ssid, sizeof(wifiConfig.ssid));
      password.toCharArray(wifiConfig.password, sizeof(wifiConfig.password));
      xQueueSend(esp32s3::s_wifiConfigQueue, &wifiConfig, 0);
      esp32s3::sendNotify("PROV:STATE:CONNECTING");
    }
    else
    {
      esp32s3::sendNotify("PROV:ERR:BAD_FORMAT");
    }
    return true;
  }

  if (data.equals("PROV:CLEAR"))
  {
    esp32s3::WiFiConfig wifiConfig;
    wifiConfig.clearOnly = true;
    wifiConfig.ssid[0] = '\0';
    wifiConfig.password[0] = '\0';
    xQueueSend(esp32s3::s_wifiConfigQueue, &wifiConfig, 0);
    return true;
  }

  if (data.equals("PROV:GET"))
  {
    if (esp32s3::s_wifiConnected && WiFi.status() == WL_CONNECTED)
    {
      esp32s3::sendNotify("PROV:OK:IP=" + WiFi.localIP().toString());
    }
    else if (esp32s3::s_store.hasSaved())
    {
      esp32s3::sendNotify("PROV:STATE:SAVED");
    }
    else
    {
      esp32s3::sendNotify("PROV:STATE:IDLE");
    }
    return true;
  }

  return false;
}

bool esp32s3::WifiModule::isConnected()
{
  return esp32s3::s_wifiConnected && WiFi.status() == WL_CONNECTED;
}
