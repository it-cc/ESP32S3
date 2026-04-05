#ifndef ULTRASONIC_SERVICE_H
#define ULTRASONIC_SERVICE_H

#include <Arduino.h>

// 注意：ESP32-S3 的 GPIO26~GPIO37 保留给 SPI Flash/PSRAM，请勿使用！
// 建议使用 GPIO 1/2/3/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/38/39/40/41/42/43/44/45/46/47/48

#ifndef ULTRASONIC_TRIG_PIN0
#define ULTRASONIC_TRIG_PIN0 1
#endif

#ifndef ULTRASONIC_ECHO_PIN0
#define ULTRASONIC_ECHO_PIN0 2
#endif

#ifndef ULTRASONIC_TRIG_PIN1
#define ULTRASONIC_TRIG_PIN1 3
#endif

#ifndef ULTRASONIC_ECHO_PIN1
#define ULTRASONIC_ECHO_PIN1 6
#endif

namespace esp32s3
{
  class UltrasonicModule
  {
  public:
    static bool init0(uint8_t trigPin = ULTRASONIC_TRIG_PIN0,
                            uint8_t echoPin = ULTRASONIC_ECHO_PIN0);
    static bool init1(uint8_t trigPin = ULTRASONIC_TRIG_PIN1,
                            uint8_t echoPin = ULTRASONIC_ECHO_PIN1);

    static bool startTask();
    static void begin0(uint8_t trigPin = ULTRASONIC_TRIG_PIN0,
            uint8_t echoPin = ULTRASONIC_ECHO_PIN0);
    static float measureDistanceMm0();
    static float getLatestDistanceMm0();

    static void begin1(uint8_t trigPin = ULTRASONIC_TRIG_PIN1,
            uint8_t echoPin = ULTRASONIC_ECHO_PIN1);
    static float measureDistanceMm1();
    static float getLatestDistanceMm1();

    static void task(void* pvParameters);
  };
}  // namespace esp32s3

#endif
