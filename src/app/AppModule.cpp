#include "app/AppModule.h"

#include "WIFI/WifiModule.h"
#include "ble/BleModule.h"
#include "camera/CameraModule.h"
#include "iic/IicMasterModule.h"
#include "motor/MotorModule.h"
#include "ultrasonic/ultrasonic.h"

namespace esp32s3
{
namespace
{
class BootCoordinator
{
 public:
  static void forwardWifiNotifyToBle(const String& message)
  {
    BleModule::sendMessage(message);
  }

  static bool handleWifiBleBridgeCommand(const String& data)
  {
    if (CameraModule::handleBleCommand(data))
    {
      return true;
    }
    return WifiModule::handleBleCommand(data);
  }
};
}  // namespace

bool AppModule::boot()
{
  bool motorInitOk = MotorModule::init();
  bool bleInitOk = BleModule::init();
  bool wifiInitOk = WifiModule::init(BootCoordinator::forwardWifiNotifyToBle);
  bool ultrasonicInitOk =
      UltrasonicModule::init(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);
  bool cameraInitOk = CameraModule::init();
  bool iicInitOk = IicMasterModule::init();

  BleModule::registerExternalCommandHandler(
      BootCoordinator::handleWifiBleBridgeCommand);

  bool motorTaskOk = false;
  bool bleTaskOk = false;
  bool wifiTaskOk = false;
  bool ultrasonicTaskOk = false;
  bool cameraTaskOk = false;
  bool iicTaskOk = false;

  if (motorInitOk)
  {
    motorTaskOk = MotorModule::startTasks();
  }
  if (bleInitOk)
  {
    bleTaskOk = BleModule::startTasks();
  }
  if (wifiInitOk)
  {
    wifiTaskOk = WifiModule::startTask(1, 0, 4096);
  }
  if (ultrasonicInitOk)
  {
    ultrasonicTaskOk = UltrasonicModule::startTask();
  }
  if (cameraInitOk)
  {
    cameraTaskOk = CameraModule::startTasks();
  }
  if (iicInitOk)
  {
    iicTaskOk = IicMasterModule::startTask(1, 0, 4096);
  }

  return motorInitOk && bleInitOk && wifiInitOk && ultrasonicInitOk &&
         cameraInitOk && iicInitOk && motorTaskOk && bleTaskOk && wifiTaskOk &&
         ultrasonicTaskOk && cameraTaskOk && iicTaskOk;
}
}  // namespace esp32s3
