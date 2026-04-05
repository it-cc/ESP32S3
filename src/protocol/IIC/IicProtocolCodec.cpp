#include "protocol/IIC/IicProtocolCodec.h"

namespace esp32s3
{
namespace iic
{
uint8_t IicProtocolCodec::crc8(const uint8_t* data, size_t len)
{
  if (data == nullptr)
  {
    return 0;
  }

  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; ++i)
  {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      if (crc & 0x80)
      {
        crc = (uint8_t)((crc << 1) ^ 0x31);
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}

bool IicProtocolCodec::encode(uint8_t msgType, uint8_t seq,
                              const uint8_t* payload, uint8_t payloadLen,
                              uint8_t* outBuf, size_t outBufSize,
                              size_t* outLen)
{
  if (outLen != nullptr)
  {
    *outLen = 0;
  }

  if (outBuf == nullptr)
  {
    return false;
  }

  if (payloadLen > kMaxPayloadLen)
  {
    return false;
  }

  const size_t totalLen = kMinFrameLen + payloadLen;
  if (outBufSize < totalLen)
  {
    return false;
  }

  outBuf[0] = kProtocolVersion;
  outBuf[1] = msgType;
  outBuf[2] = seq;
  outBuf[3] = payloadLen;

  if (payloadLen > 0)
  {
    if (payload == nullptr)
    {
      return false;
    }

    for (uint8_t i = 0; i < payloadLen; ++i)
    {
      outBuf[4 + i] = payload[i];
    }
  }

  outBuf[4 + payloadLen] = crc8(outBuf, 4 + payloadLen);

  if (outLen != nullptr)
  {
    *outLen = totalLen;
  }

  return true;
}

bool IicProtocolCodec::decode(const uint8_t* data, size_t len,
                              FrameView* outFrame, ErrorCode* outErr)
{
  if (outErr != nullptr)
  {
    *outErr = ERR_OK;
  }

  if (data == nullptr || outFrame == nullptr)
  {
    if (outErr != nullptr)
    {
      *outErr = ERR_BAD_LEN;
    }
    return false;
  }

  if (len < kMinFrameLen || len > kMaxFrameLen)
  {
    if (outErr != nullptr)
    {
      *outErr = ERR_BAD_LEN;
    }
    return false;
  }

  const uint8_t payloadLen = data[3];
  const size_t expectedLen = kMinFrameLen + payloadLen;
  if (payloadLen > kMaxPayloadLen || len != expectedLen)
  {
    if (outErr != nullptr)
    {
      *outErr = ERR_BAD_LEN;
    }
    return false;
  }

  const uint8_t expectedCrc = crc8(data, expectedLen - 1);
  const uint8_t frameCrc = data[expectedLen - 1];
  if (expectedCrc != frameCrc)
  {
    if (outErr != nullptr)
    {
      *outErr = ERR_BAD_CRC;
    }
    return false;
  }

  outFrame->version = data[0];
  outFrame->msgType = data[1];
  outFrame->seq = data[2];
  outFrame->payloadLen = payloadLen;

  for (uint8_t i = 0; i < payloadLen; ++i)
  {
    outFrame->payload[i] = data[4 + i];
  }

  outFrame->crc8 = frameCrc;
  return true;
}
}  // namespace iic
}  // namespace esp32s3