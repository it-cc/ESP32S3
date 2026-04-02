#ifndef CAMERA_MODULE_H
#define CAMERA_MODULE_H

namespace esp32s3
{
class CameraModule
{
 public:
  static bool init();
  static bool startTasks();
};
}  // namespace esp32s3

#endif