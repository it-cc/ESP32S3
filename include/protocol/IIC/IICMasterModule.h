#ifndef IIC_MASTER_MODULE_H
#define IIC_MASTER_MODULE_H

#include <cstdint>

#include "protocol/IIC/CameraNodeManager.h"

namespace esp32s3
{
class IICMasterModule
{
 public:
  IICMasterModule(uint8_t sdaPin, uint8_t sclPin, uint8_t addrA, uint8_t addrB);
  ~IICMasterModule() = default;
  void sendMessage(uint8_t slaveAddress, uint8_t command);
  CameraStatus* requestStatus(uint8_t slaveAddress);
};
}  // namespace esp32s3

#endif