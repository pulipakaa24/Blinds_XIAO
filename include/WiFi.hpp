#ifndef WIFI_H
#define WIFI_H

#include "esp_wifi.h"
#include <string>

class WiFi {
  public:
  static void init();
  static bool attemptConnect(char *SSID, char *PW, wifi_auth_mode_t authMode);
  static bool attemptConnectEnterprise(char *SSID, char *username, 
    char *PW, wifi_auth_mode_t authMode);
  private:
  static bool authFailed;
  static bool awaitConnected();
  static esp_event_handler_instance_t instance_any_id;
  static esp_event_handler_instance_t instance_got_ip;
  static EventGroupHandle_t s_wifi_event_group;
  static esp_netif_t* netif;
  static void event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data);
  static bool isConnected();
  static std::string getIP();
};

void scanAndUpdateSSIDList();

#endif