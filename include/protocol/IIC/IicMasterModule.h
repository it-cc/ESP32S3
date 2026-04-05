#ifndef IIC_MASTER_MODULE_H
#define IIC_MASTER_MODULE_H

#include <stdint.h>

namespace esp32s3
{
namespace iic
{
struct NodeStatus
{
  uint8_t address;
  bool online;
  uint32_t lastSeenMs;
  uint8_t lastError;
  uint8_t lastSeq;
  uint16_t lastFrameCounter;
};
}  // namespace iic

class IicMasterModule
{
 public:
  static bool init();
  static bool startTasks();

  static bool pingNode(uint8_t address);
  static bool triggerCapture(uint8_t address);
  static bool rebootNode(uint8_t address);
  static bool setWifiConfig(uint8_t address, const char* ssid,
                            const char* password);
  static bool setWebSocketConfig(uint8_t address, const char* host,
                                 uint16_t port, const char* path, bool useTls);
  static bool sendProvisionConfig(uint8_t address, const char* ssid,
                                  const char* wsUrl, const char* password,
                                  int userId, int cameraId);

  static bool getNodeStatus(uint8_t address, iic::NodeStatus* outStatus);
};
}  // namespace esp32s3

#endif