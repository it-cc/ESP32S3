#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

namespace esp32s3
{
class CameraModule
{
 public:
  static bool init();
  static bool startTasks();
  static bool handleBleCommand(const String& data);
};
}  // namespace esp32s3

#endif