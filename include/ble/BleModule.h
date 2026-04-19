#ifndef BLE_MODULE_H
#define BLE_MODULE_H

#include <Arduino.h>

namespace esp32s3
{
typedef bool (*BleExternalCommandHandler)(const String& data);

class BleModule
{
 public:
  static bool init();
  static bool startTasks();
  static void sendMessage(const String& message);
  static void registerExternalCommandHandler(BleExternalCommandHandler handler);
  static int getUserId();
};
}  // namespace esp32s3

#endif
