#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <Arduino.h>

namespace esp32s3
{
typedef void (*WifiNotifyCallback)(const String& message);

class WifiModule
{
 public:
  static bool init(WifiNotifyCallback notifyCb);
  static bool startTask(UBaseType_t priority = 1, BaseType_t core = 0,
                        uint32_t stackWords = 4096);
  static bool handleBleCommand(const String& data);
  static bool isConnected();
};
}  // namespace esp32s3

#endif
