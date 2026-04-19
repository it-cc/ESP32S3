#include "protocol/http/http_client.h"

#include <WiFi.h>

#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esp32camera
{
cameraClient::cameraClient(const clientConfig &clientCfg)
    : clientCfg_(clientCfg)
{
  xTaskCreate(
      [](void *param)
      {
        cameraClient *client = static_cast<cameraClient *>(param);
        String response;
        int statusCode;
        unsigned long lastPrintTime = 0;
        while (true)
        {
          bool success = client->postRequest(response, statusCode);

          // 使用 millis() 限制打印频率，比如每 1000ms (1秒) 输出一次
          if (millis() - lastPrintTime >= 1000)
          {
            lastPrintTime = millis();
            if (success)
            {
              Serial.println("Request successful!");
              Serial.printf("Response: %s\n", response.c_str());
              Serial.printf("Status Code: %d\n", statusCode);
            }
            else
            {
              Serial.println("Request failed!");
              Serial.printf("Response: %s\n", response.c_str());
              Serial.printf("Status Code: %d\n", statusCode);
            }
          }
          vTaskDelay(pdMS_TO_TICKS(33));
        }
      },
      "HTTPClientTask", 4096, this, 1, NULL);
}

bool cameraClient::postRequest(String &responseOut, int &statusCodeOut)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    statusCodeOut = -1;
    responseOut = "wifi not connected";
    return false;
  }
  String url = clientCfg_.serverURL + "?" + clientCfg_.queryKey + "=" +
               String(clientCfg_.queryValue);
  http_.begin(url);
  http_.addHeader("authorization", clientCfg_.headValue);
  http_.addHeader("Content-Type", "image/jpeg");

  camera_fb_t *fb = esp_camera_fb_get();

  int httpResponseCode = http_.POST(fb->buf, fb->len);

  esp_camera_fb_return(fb);

  if (httpResponseCode > 0)
  {
    statusCodeOut = httpResponseCode;
    responseOut = http_.getString();
    http_.end();
    return true;
  }
  else
  {
    statusCodeOut = httpResponseCode;
    responseOut = http_.errorToString(httpResponseCode);
    http_.end();
    return false;
  }
}
}  // namespace esp32camera
