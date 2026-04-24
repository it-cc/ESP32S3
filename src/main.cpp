#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>

#include "LogSwitch.h"
#include "WIFI/WifiCredentialStore.h"
#include "WIFI/WifiModule.h"
#include "app/AppModule.h"
#include "ble/BleModule.h"
#include "camera/CameraWebserver.h"
#include "config/app_config.h"
#include "protocol/IIC/IIC_camera.h"
#include "protocol/http/http_client.h"
#include "protocol/webSocket/webSocket_client.h"

#define CAMERA_IIC_ADDRESS 0x42
#define IIC_SCL_PIN 19
#define IIC_SDA_PIN 20
#define IIC_FREQUENCY 100000

void sendMsgToCamera(esp32s3::Camera_IIC& targetIic, uint8_t userID,
                     const String& ssid, const String& password)
{
  esp32s3::CameraPackage cameraPacket(userID, ssid.c_str(), password.c_str());
  byte error;
  do
  {
    error = targetIic.sendPacket(cameraPacket);
    delay(500);
  } while (error != 0);
  uint8_t status;
  do
  {
    status = targetIic.requestStatus();
    delay(500);
  } while (status != 0x00);
  targetIic.end();
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 初始化项目中各模块（包含配置WIFI与低功耗蓝牙服务）
  esp32s3::AppModule::boot();

  // 等待读取到历史WIFI或蓝牙配网输入
  String ssid = esp32s3::WifiModule::getCurrentSsid();
  String password = esp32s3::WifiModule::getCurrentPassword();
  while (ssid.isEmpty())
  {
    ssid = esp32s3::WifiModule::getCurrentSsid();
    password = esp32s3::WifiModule::getCurrentPassword();
    delay(500);
  }

  {
    esp32s3::Camera_IIC iic1(IIC_SDA_PIN, IIC_SCL_PIN, IIC_FREQUENCY,
                             CAMERA_IIC_ADDRESS);
    sendMsgToCamera(iic1, esp32s3::BleModule::getUserId(), ssid, password);
  }

  bool cameraInitOk = cameraInit(false);
  if (cameraInitOk)
  {
    static esp32camera::WebsocketClient webSocketClient(
        esp32camera::webSocket_host, esp32camera::webSocket_port,
        esp32camera::webSocket_path);
  }
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  vTaskDelay(pdMS_TO_TICKS(1000));
}
