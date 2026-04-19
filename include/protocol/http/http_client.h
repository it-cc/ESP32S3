#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>

namespace esp32camera
{
struct clientConfig
{
  String serverURL;
  String headValue;
  String queryKey;
  int queryValue;
};

class cameraClient
{
 public:
  explicit cameraClient(const clientConfig& clientCfg);
  bool postRequest(String& responseOut, int& statusCodeOut);

 private:
  HTTPClient http_;
  clientConfig clientCfg_;
};
}  // namespace esp32camera

#endif  // HTTP_CLIENT_H
