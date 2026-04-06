#include "protocol/IIC/IIC_camera.h"

namespace esp32s3
{

Camera_IIC::Camera_IIC(int sdaPin, int sclPin, uint32_t frequency, int address)
    : isAllReady_(false), address_(address)
{
  Wire.begin(sdaPin, sclPin, frequency);
}

byte Camera_IIC::sendPacket(const CameraPackage& cameraPacket)
{
  Wire.beginTransmission(address_);
  Wire.write((uint8_t*)&cameraPacket, sizeof(CameraPackage));
  byte error = Wire.endTransmission();
  searchError(error);
  return error;
}

uint8_t Camera_IIC::requestStatus()
{
  uint8_t bytesReceived = Wire.requestFrom(address_, sizeof(SlaveStatus));
  if (bytesReceived == 0)
  {
    Serial.println("No data received from IIC slave.");
    return 0x01;
  }
  else if (bytesReceived < sizeof(SlaveStatus))
  {
    Serial.println("Incomplete data received from IIC slave.");
    return 0x02;
  }
  else if (bytesReceived == sizeof(SlaveStatus))
  {
    uint8_t* p = (uint8_t*)&slaveStatus_;

    while (Wire.available())
    {
      *p++ = Wire.read();
    }
  }
  if (slaveStatus_.isReceived == 0x01 && slaveStatus_.isSetWifi == 0x02 &&
      slaveStatus_.isgetUserID == 0x04)
  {
    isAllReady_ = true;
    Serial.println("IIC slave status: all ready.");
    return 0x00;
  }
  Serial.println("IIC slave status: not all ready.");
  return 0x03;  // status error
}

void Camera_IIC::searchError(byte error)
{
  switch (error)
  {
    case 0:
      Serial.println("IIC packet sent successfully.");
      break;
    case 1:
      Serial.println("IIC data too long to fit in transmit buffer.");
      break;
    case 2:
      Serial.println("IIC received NACK on transmit of address.");
      break;
    case 3:
      Serial.println("IIC received NACK on transmit of data.");
      break;
    case 4:
      Serial.println("IIC other error.");
      break;
    case 5:
      Serial.println("IIC transmission timeout.");
      break;
    default:
      Serial.println("IIC unknown error.");
      break;
  }
}
}  // namespace esp32s3