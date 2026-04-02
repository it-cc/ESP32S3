#ifndef MOTOR_MODULE_H
#define MOTOR_MODULE_H

namespace esp32s3
{
class MotorModule
{
 public:
  static bool init();
  static bool startTasks();
  static bool startByAngle(int angle);
  static bool stop();
};
}  // namespace esp32s3

#endif
