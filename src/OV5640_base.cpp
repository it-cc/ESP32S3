#include "OV5640_base.h"

esp_err_t camera_init()
{
  // power up the camera if PWDN pin is defined
  if (CAM_PIN_PWDN != -1)
  {
    pinMode(CAM_PIN_PWDN, OUTPUT);
    digitalWrite(CAM_PIN_PWDN, LOW);
  }

  // initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK)
  {
    ESP_LOGE("OV5640_base", "Camera Init Failed");
    return err;
  }

  ESP_LOGI("OV5640_base", "Camera init success");

  return ESP_OK;
}

esp_err_t camera_capture()
{
  // acquire a frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb)
  {
    ESP_LOGE("OV5640_base", "Camera Capture Failed");
    return ESP_FAIL;
  }
  save_photo(fb);

  // return the frame buffer back to the driver for reuse
  esp_camera_fb_return(fb);
  return ESP_OK;
}

void save_photo(camera_fb_t* fb)
{
  if (!fb)
  {
    ESP_LOGE("funtion:save_photo", "捕获失败");
    return;
  }

  if (fb->format != PIXFORMAT_JPEG)
  {
    ESP_LOGE("funtion:save_photo", "格式不是 JPEG,无法直接保存");
    return;
  }

  // 3. 创建文件并写入
  char path[64];
  uint64_t ts = (uint64_t)(esp_timer_get_time() / 1000ULL);
  snprintf(path, sizeof(path), "/picture_%llu.jpg", (unsigned long long)ts);

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file)
  {
    ESP_LOGE("funtion:save_photo", "创建文件失败");
  }
  else
  {
    // 直接将 buf 中的二进制数据写入文件
    file.write(fb->buf, fb->len);
    ESP_LOGI("funtion:save_photo", "照片已保存: %s, 大小: %zu 字节", path,
             fb->len);
    file.close();
  }
}