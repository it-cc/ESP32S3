#include "camera/CameraModule.h"

#include <Arduino.h>
#include <WiFi.h>

#include "LogSwitch.h"
#include "camera/CameraService.h"
#include "camera/MemoryPhotoStore.h"
#include "camera/PhotoWebServer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esp32s3
{
static const uint16_t WEB_SERVER_PORT = 80;

static CameraService g_camera;
static MemoryPhotoStore g_photoStore(6);
static PhotoWebServer g_photoWeb(g_photoStore);

static const uint32_t CAPTURE_INTERVAL_MS = 41;
static const uint32_t WEB_TASK_STACK = 8192;
static const uint32_t CAPTURE_TASK_STACK = 8192;

static bool s_initialized = false;

static void webTask(void* pvParameters)
{
  LOG_PRINTLN(LOG_CAMERA, "WebTask: starting...");

  while (WiFi.status() != WL_CONNECTED)
  {
    LOG_PRINTLN(LOG_CAMERA,
                "WebTask: waiting for WiFi provisioning/connection...");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  LOG_PRINT(LOG_CAMERA, "WebTask: WiFi connected, IP=");
  LOG_PRINTLN(LOG_CAMERA, WiFi.localIP());

  if (!g_photoWeb.begin(WEB_SERVER_PORT))
  {
    LOG_PRINTLN(LOG_CAMERA, "WebTask: web server start failed");
    vTaskDelete(NULL);
    return;
  }

  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void captureTask(void* pvParameters)
{
  for (;;)
  {
    uint8_t* buf = NULL;
    size_t len = 0;
    uint64_t tsMs = 0;

    if (g_camera.captureToJpegBuffer(&buf, &len, &tsMs) != ESP_OK)
    {
      LOG_PRINTLN(LOG_CAMERA, "CaptureTask: capture failed");
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    if (!g_photoStore.pushOwnedFrame(buf, len, tsMs))
    {
      LOG_PRINTLN(LOG_CAMERA, "CaptureTask: ring buffer busy, drop frame");
      free(buf);
    }
    else
    {
      g_photoWeb.notifyNewFrame();
    }

    vTaskDelay(pdMS_TO_TICKS(CAPTURE_INTERVAL_MS));
  }
}
}  // namespace esp32s3

bool esp32s3::CameraModule::init()
{
  if (!esp32s3::g_photoStore.begin())
  {
    LOG_PRINTLN(LOG_CAMERA, "Photo store init failed");
    return false;
  }

  if (esp32s3::g_camera.begin() != ESP_OK)
  {
    LOG_PRINTLN(LOG_CAMERA, "Camera init failed");
    return false;
  }

  LOG_PRINTLN(LOG_CAMERA, "Camera init success");
  esp32s3::s_initialized = true;
  return true;
}

bool esp32s3::CameraModule::startTasks()
{
  if (!esp32s3::s_initialized)
  {
    LOG_PRINTLN(LOG_CAMERA, "camera init required before start tasks");
    return false;
  }

  BaseType_t webOk = xTaskCreatePinnedToCore(
      esp32s3::webTask, "WebTask", esp32s3::WEB_TASK_STACK, NULL, 1, NULL, 0);
  BaseType_t capOk =
      xTaskCreatePinnedToCore(esp32s3::captureTask, "CaptureTask",
                              esp32s3::CAPTURE_TASK_STACK, NULL, 1, NULL, 1);

  if (webOk != pdPASS)
  {
    LOG_PRINTLN(LOG_CAMERA, "camera WebTask create failed");
  }
  if (capOk != pdPASS)
  {
    LOG_PRINTLN(LOG_CAMERA, "camera CaptureTask create failed");
  }
  if (webOk != pdPASS || capOk != pdPASS)
  {
    LOG_PRINTF(LOG_CAMERA, "camera free heap: %u\n", ESP.getFreeHeap());
    LOG_PRINTF(LOG_CAMERA, "camera min free heap: %u\n", ESP.getMinFreeHeap());
    return false;
  }

  return true;
}

bool esp32s3::CameraModule::handleBleCommand(const String& data)
{
  if (!data.startsWith("id:"))
  {
    return false;
  }

  String idStr = data.substring(3);
  if (idStr.length() == 0)
  {
    LOG_PRINTLN(LOG_CAMERA, "[Camera] invalid id command: empty id");
    return true;
  }

  for (size_t i = 0; i < idStr.length(); ++i)
  {
    char c = idStr.charAt(i);
    if (c < '0' || c > '9')
    {
      LOG_PRINTLN(LOG_CAMERA, "[Camera] invalid id command: non-digit");
      return true;
    }
  }

  uint32_t userId = (uint32_t)idStr.toInt();
  esp32s3::g_photoWeb.setUserId(userId);
  LOG_PRINTF(LOG_CAMERA, "[Camera] user id updated via BLE: %u\n",
             (unsigned int)userId);
  return true;
}
