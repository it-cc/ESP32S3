#include <Arduino.h>

#include "LogSwitch.h"
#include "app/AppModule.h"

void setup()
{
  Serial.begin(115200);
  delay(1000);
  if (!esp32s3::AppModule::boot())
  {
    LOG_PRINTLN(LOG_CAMERA, "[Main] module init/start has failures");
  }
}

void loop()
{
  // 避免空转占满 CPU，保证系统任务调度稳定
  vTaskDelay(pdMS_TO_TICKS(1000));
}
