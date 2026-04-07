#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>

#include "LogSwitch.h"
#include "WIFI/WifiCredentialStore.h"
#include "app/AppModule.h"
#include "protocol/IIC/IIC_camera.h"

#define CAMERA1_IIC_ADDRESS 0x42
#define CAMERA2_IIC_ADDRESS 0x43
#define IIC_SCL_PIN 9
#define IIC_SDA_PIN 8
#define IIC_FREQUENCY 100000
void setup()
{
  Serial.begin(115200);
  delay(1000);
  // Keep the IIC master object alive for the full application lifetime.
  static esp32s3::Camera_IIC iic(IIC_SDA_PIN, IIC_SCL_PIN, IIC_FREQUENCY,
                                 CAMERA1_IIC_ADDRESS);
  esp32s3::CameraPackage cameraPacket(1, "Nanami", "0d000721");
  byte error;
  do
  {
    error = iic.sendPacket(cameraPacket);
    delay(500);
  } while (error != 0);
  uint8_t status;
  do
  {
    status = iic.requestStatus();
    delay(500);
  } while (status != 0x00);
  Serial.println(iic.getSlaveStatus().ssid);
  Serial.println(iic.getSlaveStatus().password);
  Serial.println("httpUrl: " + String(iic.getSlaveStatus().httpUrl));
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  Serial.println("setup begin");
  vTaskDelay(pdMS_TO_TICKS(1000));
}
