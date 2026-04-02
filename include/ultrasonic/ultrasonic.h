#ifndef ULTRASONIC_SERVICE_H
#define ULTRASONIC_SERVICE_H

#include <Arduino.h>

#ifndef ULTRASONIC_TRIG_PIN
#define ULTRASONIC_TRIG_PIN 47
#endif

#ifndef ULTRASONIC_ECHO_PIN
#define ULTRASONIC_ECHO_PIN 48
#endif

namespace esp32s3
{
class UltrasonicModule
{
 public:
  static bool init(uint8_t trigPin = ULTRASONIC_TRIG_PIN,
                   uint8_t echoPin = ULTRASONIC_ECHO_PIN);
  static bool startTask();
  static void begin(uint8_t trigPin = ULTRASONIC_TRIG_PIN,
                    uint8_t echoPin = ULTRASONIC_ECHO_PIN);
  static float measureDistanceMm();
  static float getLatestDistanceMm();
  static void task(void* pvParameters);
};
}  // namespace esp32s3

#endif