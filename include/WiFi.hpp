#ifndef WIFI_H
#define WIFI_H

#include "esp_wifi.h"
#include <string>

class WiFi {
  public:
  static void init();
  static bool attemptConnect(const std::string ssid, const std::string password,
    const wifi_auth_mode_t authMode);
  static bool attemptConnect(const std::string ssid, const std::string uname,
    const std::string password, const wifi_auth_mode_t authMode);
  static bool isConnected();
  static void scanAndUpdateSSIDList();
  private:
  static void processScanResults();
  static std::atomic<bool> authFailed;
  static bool awaitConnected();
  static esp_event_handler_instance_t instance_any_id;
  static esp_event_handler_instance_t instance_got_ip;
  static EventGroupHandle_t s_wifi_event_group;
  static esp_netif_t* netif;
  static void event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data);
  static std::string getIP();
};

extern WiFi bmWiFi;

void scanAndUpdateSSIDList();

#endif