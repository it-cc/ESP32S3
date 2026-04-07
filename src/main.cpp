#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>

#include "LogSwitch.h"
#include "WIFI/WifiCredentialStore.h"
#include "WIFI/WifiModule.h"
#include "app/AppModule.h"
#include "protocol/IIC/IIC_camera.h"

#define CAMERA1_IIC_ADDRESS 0x42
#define CAMERA2_IIC_ADDRESS 0x43
#define IIC_SCL_PIN 9
#define IIC_SDA_PIN 8
#define IIC_FREQUENCY 100000

esp32s3::Camera_IIC iic1(IIC_SDA_PIN, IIC_SCL_PIN, IIC_FREQUENCY,
                         CAMERA1_IIC_ADDRESS);
esp32s3::Camera_IIC iic2(IIC_SDA_PIN, IIC_SCL_PIN, IIC_FREQUENCY,
                         CAMERA2_IIC_ADDRESS);

void sendWifiToCamera(esp32s3::Camera_IIC& targetIic, uint8_t userId,
                      const String& ssid, const String& password)
{
  esp32s3::CameraPackage cameraPacket(userId, ssid.c_str(), password.c_str());
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

  sendWifiToCamera(iic1, 1, ssid, password);
  Serial.println("Camera 1 Ready: ");
  Serial.println("httpUrl 1: " + String(iic1.getSlaveStatus().httpUrl));

  sendWifiToCamera(iic2, 2, ssid, password);
  Serial.println("Camera 2 Ready: ");
  Serial.println("httpUrl 2: " + String(iic2.getSlaveStatus().httpUrl));
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  vTaskDelay(pdMS_TO_TICKS(1000));
}
