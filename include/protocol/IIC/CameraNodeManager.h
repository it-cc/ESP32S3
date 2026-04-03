#ifndef CAMERA_NODE_MANAGER_H
#define CAMERA_NODE_MANAGER_H

#include <cstdint>
#include <vector>
namespace esp32s3
{
struct CameraStatus
{
  uint8_t online;
  uint8_t address;
};

class CameraNodeManager
{
 public:
  static constexpr uint8_t cameraCount = 2;

  CameraNodeManager(uint8_t address1, uint8_t address2);
  uint8_t getAddress(uint8_t index) const;
  CameraStatus getState(uint8_t index) const;

 private:
  std::vector<CameraStatus> status_;
};
}  // namespace esp32s3

#endif