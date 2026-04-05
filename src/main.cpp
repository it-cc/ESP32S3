#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>

#include "LogSwitch.h"
#include "WIFI/WifiCredentialStore.h"
#include "app/AppModule.h"
#include "protocol/IIC/IicMasterModule.h"

namespace
{
static const char* TAG = "MAIN";

#ifndef CAM_TARGET_ADDR_A
#define CAM_TARGET_ADDR_A 0x24
#endif

#ifndef CAM_TARGET_ADDR_B
#define CAM_TARGET_ADDR_B 0x25
#endif

struct CameraNetConfig
{
  uint8_t address;
  String wifiSsid;
  String wifiPassword;
  const char* wsUrl;
  int userId;
  int cameraId;
};

#ifndef CAM_A_WS_URL
#define CAM_A_WS_URL "ws://192.168.1.100:9001/ws"
#endif

#ifndef CAM_B_WS_URL
#define CAM_B_WS_URL "ws://192.168.1.100:9001/ws"
#endif

#ifndef CAM_A_CAMERA_ID
#define CAM_A_CAMERA_ID 1
#endif

#ifndef CAM_B_CAMERA_ID
#define CAM_B_CAMERA_ID 2
#endif

#ifndef CAM_ENABLE_SECOND_CAMERA
#define CAM_ENABLE_SECOND_CAMERA 0
#endif

#ifndef PROVISION_USE_HARDCODED_WIFI
#define PROVISION_USE_HARDCODED_WIFI 1
#endif

#ifndef PROVISION_TEST_WIFI_SSID
#define PROVISION_TEST_WIFI_SSID "Redmi"
#endif

#ifndef PROVISION_TEST_WIFI_PASSWORD
#define PROVISION_TEST_WIFI_PASSWORD "88889999"
#endif

#ifndef PROVISION_USE_HARDCODED_USER_ID
#define PROVISION_USE_HARDCODED_USER_ID 1
#endif

#ifndef PROVISION_TEST_USER_ID
#define PROVISION_TEST_USER_ID 1001
#endif

bool loadProjectProvisionData(String& outSsid, String& outPassword,
                              int& outUserId)
{
#if PROVISION_USE_HARDCODED_WIFI
  outSsid = PROVISION_TEST_WIFI_SSID;
  outPassword = PROVISION_TEST_WIFI_PASSWORD;
  if (outSsid.length() == 0)
  {
    LOG_PRINTLN(LOG_IIC, "[Main] provision skip: hardcoded wifi ssid is empty");
    return false;
  }
#else
  WifiCredentialStore store;
  if (!store.load(outSsid, outPassword))
  {
    LOG_PRINTLN(LOG_IIC,
                "[Main] provision skip: wifi credentials not found in project");
    return false;
  }
#endif

  Preferences pref;
  if (!pref.begin("app_cfg", false))
  {
#if PROVISION_USE_HARDCODED_USER_ID
    outUserId = PROVISION_TEST_USER_ID;
    LOG_PRINTF(LOG_IIC,
               "[Main] app_cfg missing, fallback hardcoded user_id=%d\n",
               outUserId);
    return true;
#else
    LOG_PRINTLN(LOG_IIC,
                "[Main] provision skip: cannot open app_cfg namespace");
    return false;
#endif
  }

  outUserId = pref.getInt("user_id", -1);
  pref.end();

  if (outUserId < 0)
  {
#if PROVISION_USE_HARDCODED_USER_ID
    outUserId = PROVISION_TEST_USER_ID;
    LOG_PRINTF(
        LOG_IIC,
        "[Main] app_cfg.user_id missing, fallback hardcoded user_id=%d\n",
        outUserId);
    return true;
#else
    LOG_PRINTLN(LOG_IIC, "[Main] provision skip: app_cfg.user_id is missing");
    return false;
#endif
  }

  return true;
}

bool checkCameraStartup(uint8_t address)
{
  // Give slave some time to finish boot and register callbacks.
  for (int i = 0; i < 5; ++i)
  {
    if (esp32s3::IicMasterModule::pingNode(address))
    {
      esp32s3::iic::NodeStatus status = {};
      if (esp32s3::IicMasterModule::getNodeStatus(address, &status))
      {
        ESP_LOGI(
            TAG,
            "CAMERA_STARTUP_OK addr=0x%02X online=%d err=%u seq=%u frame=%u",
            address, status.online ? 1 : 0, status.lastError, status.lastSeq,
            status.lastFrameCounter);
      }
      else
      {
        ESP_LOGI(TAG, "CAMERA_STARTUP_OK addr=0x%02X", address);
      }
      return true;
    }
    delay(100);
  }

  ESP_LOGE(TAG, "CAMERA_STARTUP_FAIL addr=0x%02X", address);
  return false;
}

void pushCameraNetConfigToNode(const CameraNetConfig& config)
{
  ESP_LOGI(TAG, "IIC provision start addr=0x%02X camera_id=%d", config.address,
           config.cameraId);
  LOG_PRINTF(LOG_IIC, "[Main] pushing config to camera addr=0x%02X\n",
             config.address);

  bool provisionOk = esp32s3::IicMasterModule::sendProvisionConfig(
      config.address, config.wifiSsid.c_str(), config.wsUrl,
      config.wifiPassword.c_str(), config.userId, config.cameraId);
  ESP_LOGI(TAG, "IIC provision %s addr=0x%02X", provisionOk ? "OK" : "FAIL",
           config.address);
  LOG_PRINTLN(LOG_IIC, provisionOk ? "[Main] camera provision json sent"
                                   : "[Main] camera provision json failed");
}

void pushCameraNetConfig()
{
  String projectSsid;
  String projectPassword;
  int projectUserId = -1;

  if (!loadProjectProvisionData(projectSsid, projectPassword, projectUserId))
  {
    return;
  }

  CameraNetConfig camA = {CAM_TARGET_ADDR_A, projectSsid,   projectPassword,
                          CAM_A_WS_URL,      projectUserId, CAM_A_CAMERA_ID};

  pushCameraNetConfigToNode(camA);
#if CAM_ENABLE_SECOND_CAMERA
  CameraNetConfig camB = {CAM_TARGET_ADDR_B, projectSsid,   projectPassword,
                          CAM_B_WS_URL,      projectUserId, CAM_B_CAMERA_ID};
  pushCameraNetConfigToNode(camB);
#endif
}
}  // namespace

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println("setup begin");

  bool bootOk = esp32s3::AppModule::boot();
  ESP_LOGI(TAG, "AppModule::boot => %s", bootOk ? "OK" : "FAIL");
  if (!bootOk)
  {
    LOG_PRINTLN(LOG_WIFI, "[Main] module init/start has failures");
    ESP_LOGE(TAG, "boot failed, skip provision");
    return;
  }

  // 等待 IIC 轮询任务完成至少一次在线探测。
  delay(500);
  esp32s3::IicMasterModule::scanBus();

  (void)checkCameraStartup(CAM_TARGET_ADDR_A);
#if CAM_ENABLE_SECOND_CAMERA
  (void)checkCameraStartup(CAM_TARGET_ADDR_B);
#endif

  ESP_LOGI(TAG, "start pushing IIC provision payload");
  pushCameraNetConfig();
  ESP_LOGI(TAG, "setup done");
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  vTaskDelay(pdMS_TO_TICKS(1000));
}
