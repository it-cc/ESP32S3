#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include <Arduino.h>
#include <HTTPClient.h>

namespace esp32camera
{
static const String WifiSsid = "Redmi";
static const String WifiPassword = "88889999";
static const String serverURL = "http://8.134.80.198:8899/ai/camera";
static const String headValue = "1";
static const String queryKey = "position";
static const int queryValue = 0;

static const char* webSocket_host = "8.134.80.198";
static const int webSocket_port = 8899;
const char* webSocket_path = "/ai/camera";
}  // namespace esp32camera

#endif  // APP_CONFIG_H
