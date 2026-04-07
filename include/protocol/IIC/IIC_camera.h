#ifndef CAMERA_IIC_H
#define CAMERA_IIC_H

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

namespace esp32s3
{
struct __attribute__((packed)) CameraPackage
{
  uint8_t userID;
  char ssid[32];
  char password[32];
  CameraPackage(uint8_t userID = 0, const char* ssid = "",
                const char* password = "")
      : userID(userID)
  {
    strncpy(this->ssid, ssid, sizeof(this->ssid) - 1);
    this->ssid[sizeof(this->ssid) - 1] = '\0';
    strncpy(this->password, password, sizeof(this->password) - 1);
    this->password[sizeof(this->password) - 1] = '\0';
  }
};

struct __attribute__((packed)) SlaveStatus
{
  uint8_t isReceived;   // 0x01
  uint8_t isSetWifi;    // 0x02
  uint8_t isgetUserID;  // 0x04
  char ssid[32];
  char password[32];
  char httpUrl[64];
};

class Camera_IIC
{
  CameraPackage cameraPacket_;
  bool isAllReady_;
  int address_;
  SlaveStatus slaveStatus_;

 public:
  Camera_IIC(int sdaPin, int sclPin, uint32_t frequency, int address);
  byte sendPacket(const CameraPackage& cameraPacket);
  uint8_t requestStatus();
  SlaveStatus getSlaveStatus() const;

 private:
  void searchError(byte error);
};
}  // namespace esp32s3
#endif  // CAMERA_IIC_H