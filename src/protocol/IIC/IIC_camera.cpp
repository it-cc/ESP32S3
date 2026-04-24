#include "protocol/IIC/IIC_camera.h"

namespace esp32s3
{

Camera_IIC::Camera_IIC(int sdaPin, int sclPin, uint32_t frequency, int address)
    : isAllReady_(false), address_(address)
{
  Wire1.begin(sdaPin, sclPin, frequency);
}

byte Camera_IIC::sendPacket(const CameraPackage& cameraPacket)
{
  Wire1.beginTransmission(address_);
  Wire1.write((uint8_t*)&cameraPacket, sizeof(CameraPackage));
  byte error = Wire1.endTransmission();
  searchError(error);
  return error;
}

uint8_t Camera_IIC::requestStatus()
{
  uint8_t bytesReceived = Wire1.requestFrom(address_, sizeof(SlaveStatus));
  if (bytesReceived == 0)
  {
    Serial.println("[request] No data received from IIC slave.");
    return 0x01;
  }
  else if (bytesReceived < sizeof(SlaveStatus))
  {
    Serial.println("[request] Incomplete data received from IIC slave.");
    return 0x02;
  }
  else if (bytesReceived == sizeof(SlaveStatus))
  {
    uint8_t* p = (uint8_t*)&slaveStatus_;

    while (Wire1.available())
    {
      *p++ = Wire1.read();
    }
  }

  if (slaveStatus_.isAllReady == 0x02)  // all ready
  {
    isAllReady_ = true;
    Serial.println("[request] IIC slave status: all ready.");
    return 0x00;
  }
  Serial.println("[request] IIC slave status: not all ready.");
  return 0x00;  // status error
}

void Camera_IIC::end() { Wire1.end(); }

SlaveStatus Camera_IIC::getSlaveStatus() const { return slaveStatus_; }

void Camera_IIC::searchError(byte error)
{
  switch (error)
  {
    case 0:
      Serial.println("[send] IIC packet sent successfully.");
      break;
    case 1:
      Serial.println("[send] IIC data too long to fit in transmit buffer.");
      break;
    case 2:
      Serial.println("[send] IIC received NACK on transmit of address.");
      break;
    case 3:
      Serial.println("[send] IIC received NACK on transmit of data.");
      break;
    case 4:
      Serial.println("[send] IIC other error.");
      break;
    case 5:
      Serial.println("[send] IIC transmission timeout.");
      break;
    default:
      Serial.println("[send] IIC unknown error.");
      break;
  }
}
}  // namespace esp32s3