#include <Arduino.h>
#include <WiFi.h>

#include "OV5640_base.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "jpg_http_capture.h"

static const char *MAIN_TAG = "main";

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 初始化文件系统
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  // 初始化摄像头
  camera_init();

  // 建立 WiFi 热点 (AP)
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32S3-CAM", "12345678");
  Serial.print("AP 启动, IP地址: ");
  Serial.println(WiFi.softAPIP());
}

void loop()
{
  // 每隔10秒拍一张照片
  Serial.println("拍摄一张新照片...");
  camera_capture();
  delay(10000);
}
