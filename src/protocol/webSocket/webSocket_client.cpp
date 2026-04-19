#include "protocol/webSocket/webSocket_client.h"

#include <memory>

#include "ble/BleModule.h"
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esp32camera
{
WebsocketClient::WebsocketClient(const char* webSocket_host,
                                 const int webSocket_port,
                                 const char* webSocket_path)
    : webSocket_()
{
  webSocket_.begin(webSocket_host, webSocket_port, webSocket_path);
  webSocket_.onEvent(WebsocketClient::webSocketEvent);
  webSocket_.setReconnectInterval(5000);
  xTaskCreate(
      [](void* param)
      {
        WebsocketClient* client = static_cast<WebsocketClient*>(param);
        while (true)
        {
          client->webSocket_.loop();
          static uint32_t last_run = 0;
          if (millis() - last_run >= 33)
          {
            last_run = millis();
            client->run();
          }
          vTaskDelay(pdMS_TO_TICKS(1));
        }
      },
      "WebsocketClientTask", 8192, this, 1, nullptr);
}

void WebsocketClient::webSocketEvent(WStype_t type, uint8_t* payload,
                                     size_t length)
{
  switch (type)
  {
    case WStype_DISCONNECTED:
      Serial.println("Websocket Disconnected");
      break;
    case WStype_CONNECTED:
      Serial.println("Websocket Connected");
      break;
    case WStype_TEXT:
      Serial.printf("Websocket Received Text: %s\n", payload);
      break;
    case WStype_BIN:
      Serial.printf("Websocket Received Binary data of length: %u\n", length);
      break;
    default:
      break;
  }
}

void WebsocketClient::run()
{
  if (webSocket_.isConnected())
  {
    int userId = esp32s3::BleModule::getUserId();
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"authorization\":%d,\"position\":\"0\"}", userId);
    webSocket_.sendTXT(payload);  // JSON
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb)
    {
      webSocket_.sendBIN(fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
    else
    {
      Serial.println("Camera capture failed");
    }
  }
  else
  {
    Serial.println("Websocket not connected");
  }
}

}  // namespace esp32camera