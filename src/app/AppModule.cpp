#include "app/AppModule.h"

#include "WIFI/WifiModule.h"
#include "ble/BleModule.h"
#include "motor/MotorModule.h"
#include "protocol/IIC/IicMasterModule.h"
#include "ultrasonic/ultrasonic.h"

namespace esp32s3
{
namespace
{
class BootCoordinator
{
 public:
  static bool parseNodeAddress(const String& text, uint8_t* outAddress)
  {
    if (outAddress == nullptr)
    {
      return false;
    }

    String value = text;
    value.trim();
    if (value.length() == 0)
    {
      return false;
    }

    long parsed = -1;
    if (value.startsWith("0x") || value.startsWith("0X"))
    {
      parsed = strtol(value.c_str(), nullptr, 16);
    }
    else
    {
      parsed = strtol(value.c_str(), nullptr, 10);
    }

    if (parsed < 0x08 || parsed > 0x77)
    {
      return false;
    }

    *outAddress = (uint8_t)parsed;
    return true;
  }

  static bool handleIicCommand(const String& data)
  {
    const String cmdPing = "IIC_PING:";
    const String cmdCapture = "IIC_CAPTURE:";
    const String cmdReboot = "IIC_REBOOT:";
    const String cmdStatus = "IIC_STATUS:";

    uint8_t address = 0;

    if (data.startsWith(cmdPing))
    {
      if (!parseNodeAddress(data.substring(cmdPing.length()), &address))
      {
        BleModule::sendMessage("IIC_PING_FAIL:BAD_ADDR");
        return true;
      }

      bool ok = IicMasterModule::pingNode(address);
      BleModule::sendMessage(ok ? "IIC_PING_OK" : "IIC_PING_FAIL");
      return true;
    }

    if (data.startsWith(cmdCapture))
    {
      if (!parseNodeAddress(data.substring(cmdCapture.length()), &address))
      {
        BleModule::sendMessage("IIC_CAPTURE_FAIL:BAD_ADDR");
        return true;
      }

      bool ok = IicMasterModule::triggerCapture(address);
      BleModule::sendMessage(ok ? "IIC_CAPTURE_OK" : "IIC_CAPTURE_FAIL");
      return true;
    }

    if (data.startsWith(cmdReboot))
    {
      if (!parseNodeAddress(data.substring(cmdReboot.length()), &address))
      {
        BleModule::sendMessage("IIC_REBOOT_FAIL:BAD_ADDR");
        return true;
      }

      bool ok = IicMasterModule::rebootNode(address);
      BleModule::sendMessage(ok ? "IIC_REBOOT_OK" : "IIC_REBOOT_FAIL");
      return true;
    }

    if (data.startsWith(cmdStatus))
    {
      if (!parseNodeAddress(data.substring(cmdStatus.length()), &address))
      {
        BleModule::sendMessage("IIC_STATUS_FAIL:BAD_ADDR");
        return true;
      }

      iic::NodeStatus nodeStatus = {};
      if (!IicMasterModule::getNodeStatus(address, &nodeStatus))
      {
        BleModule::sendMessage("IIC_STATUS_FAIL:NO_NODE");
        return true;
      }

      String message = "IIC_STATUS:";
      message += String(nodeStatus.address);
      message += ",online=";
      message += (nodeStatus.online ? "1" : "0");
      message += ",err=";
      message += String(nodeStatus.lastError);
      message += ",seq=";
      message += String(nodeStatus.lastSeq);
      message += ",frame=";
      message += String(nodeStatus.lastFrameCounter);
      BleModule::sendMessage(message);
      return true;
    }

    return false;
  }

  static void forwardWifiNotifyToBle(const String& message)
  {
    BleModule::sendMessage(message);
  }

  static bool handleWifiBleBridgeCommand(const String& data)
  {
    if (handleIicCommand(data))
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
  bool iicInitOk = IicMasterModule::init();
  bool ultrasonicInitOk =
      UltrasonicModule::init(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);

  BleModule::registerExternalCommandHandler(
      BootCoordinator::handleWifiBleBridgeCommand);

  bool motorTaskOk = false;
  bool bleTaskOk = false;
  bool wifiTaskOk = false;
  bool iicTaskOk = false;
  bool ultrasonicTaskOk = false;

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
  if (iicInitOk)
  {
    iicTaskOk = IicMasterModule::startTasks();
  }
  if (ultrasonicInitOk)
  {
    ultrasonicTaskOk = UltrasonicModule::startTask();
  }

  return motorInitOk && bleInitOk && wifiInitOk && iicInitOk &&
         ultrasonicInitOk && motorTaskOk && bleTaskOk && wifiTaskOk &&
         iicTaskOk && ultrasonicTaskOk;
}
}  // namespace esp32s3
