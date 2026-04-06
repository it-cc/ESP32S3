#include <Arduino.h>

#include "LogSwitch.h"
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
  // if (!esp32s3::AppModule::boot())
  // {
  //   LOG_PRINTLN(LOG_WIFI, "[Main] module init/start has failures");
  // }
  // Keep the IIC master object alive for the full application lifetime.
  static esp32s3::Camera_IIC iic(IIC_SDA_PIN, IIC_SCL_PIN, IIC_FREQUENCY,
                                 CAMERA1_IIC_ADDRESS);
  esp32s3::CameraPackage cameraPacket(1, "Redmi", "88889999");
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
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  delay(100);
}
