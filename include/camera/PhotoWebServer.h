#ifndef PHOTO_WEB_SERVER_H
#define PHOTO_WEB_SERVER_H

#include <Arduino.h>

#include <vector>

#include "MemoryPhotoStore.h"
#include "esp_http_server.h"
#include "esp_log.h"

class PhotoWebServer
{
 public:
  explicit PhotoWebServer(MemoryPhotoStore& store);
  bool begin(uint16_t port = 80);
  void notifyNewFrame();
  void setUserId(uint32_t userId);
  uint32_t getUserId() const;
  uint32_t getCameraId() const;

 private:
  static esp_err_t indexHandler(httpd_req_t* req);
  static esp_err_t wsHandler(httpd_req_t* req);

  esp_err_t handleIndex(httpd_req_t* req);
  esp_err_t handleWs(httpd_req_t* req);

  bool getLatestFrame(uint8_t** outBuf, size_t* outLen, uint32_t* outId);
  bool parseAndStoreUserId(const char* text);
  bool sendMetaToClient(httpd_req_t* req, uint32_t frameId, size_t frameLen);
  esp_err_t sendLatestFrameToClient(httpd_req_t* req);
  void broadcastLatestToWsClients();
  static void wsBroadcastWork(void* arg);

  MemoryPhotoStore& store_;
  httpd_handle_t server_;
  volatile uint32_t userId_;

  static constexpr uint32_t kCameraId = 1;

  static PhotoWebServer* instance_;
};

#endif