#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <WebSocketsClient.h>

namespace esp32camera
{
class WebsocketClient
{
 public:
  WebsocketClient(const char* webSocket_host, const int webSocket_port,
                  const char* webSocket_path);
  static void webSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  void run();

 private:
  WebSocketsClient webSocket_;
  bool initialized_;
};
}  // namespace esp32camera

#endif  // WEBSOCKET_CLIENT_H