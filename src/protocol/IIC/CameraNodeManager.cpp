#include "protocol/IIC/CameraNodeManager.h"

#include <Wire.h>
#include <string.h>

namespace esp32s3
{
CameraNodeManager::CameraNodeManager(uint8_t address1, uint8_t address2)
{
  status_.resize(cameraCount);
  status_[0].online = 0;
  status_[1].online = 0;
  status_[0].address = address1;
  status_[1].address = address2;
}

uint8_t CameraNodeManager::getAddress(uint8_t index) const
{
  return status_[index].address;
}

CameraStatus CameraNodeManager::getState(uint8_t index) const
{
  return status_[index];
}
}  // namespace esp32s3