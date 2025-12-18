#include "WiFi.hpp"
#include "esp_eap_client.h"
#include "cJSON.h" // Native replacement for ArduinoJson
#include "BLE.hpp"

bool WiFi::authFailed = false;
EventGroupHandle_t WiFi::s_wifi_event_group = NULL;
esp_netif_t* WiFi::netif = NULL;
esp_event_handler_instance_t WiFi::instance_any_id = NULL;
esp_event_handler_instance_t WiFi::instance_got_ip = NULL;

#define WIFI_CONNECTED_BIT BIT0

WiFi bmWiFi;

// The Event Handler (The engine room)
void WiFi::event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
  
  // We got disconnected -> Retry automatically
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    // 1. Cast the data to the correct struct
    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
    
    printf("WiFi Disconnected. Reason Code: %d\n", event->reason);

    // 2. Check specific Reason Codes
    switch (event->reason) {
      case WIFI_REASON_AUTH_EXPIRE:               // Reason 2
      case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:    // Reason 15 (Most Common for Wrong Pass)
      case WIFI_REASON_BEACON_TIMEOUT:            // Reason 200
      case WIFI_REASON_AUTH_FAIL:                 // Reason 203
      case WIFI_REASON_ASSOC_FAIL:                // Reason 204
      case WIFI_REASON_HANDSHAKE_TIMEOUT:         // Reason 205
        printf("ERROR: Likely Wrong Password!\n");
        // OPTIONAL: Don't retry immediately if password is wrong
        authFailed = true;
        break;
          
      case WIFI_REASON_NO_AP_FOUND:               // Reason 201
        printf("ERROR: SSID Not Found\n");
        authFailed = true;
        break;
          
      default:
        printf("Retrying...\n");
        esp_wifi_connect(); // Retry for other reasons (interference, etc)
        break;
    }
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  } 
  // 3. We got an IP Address -> Success!
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    printf("Got IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void WiFi::init() {
  s_wifi_event_group = xEventGroupCreate();
  // 1. Init Network Interface
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  // 2. Init WiFi Driver
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // 3. Set Mode to Station (Client)
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    WIFI_EVENT,         // The "Topic" (Base)
    ESP_EVENT_ANY_ID,   // The specific "Subject" (Any ID)
    &event_handler,     // The Function to call
    NULL,               // Argument to pass to the function (optional)
    &instance_any_id    // Where to store the registration handle
  ));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
    IP_EVENT,           
    IP_EVENT_STA_GOT_IP, 
    &event_handler,
    NULL,
    &instance_got_ip
  ));

  ESP_ERROR_CHECK(esp_wifi_start());
}

// --- CHECK STATUS ---
bool WiFi::isConnected() {
  if (s_wifi_event_group == NULL) return false;
  EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
  return (bits & WIFI_CONNECTED_BIT);
}

// --- GET IP AS STRING ---
std::string WiFi::getIP() {
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(netif, &ip_info);
  char buf[20];
  sprintf(buf, IPSTR, IP2STR(&ip_info.ip));
  return std::string(buf);
}

bool WiFi::attemptConnect(const std::string ssid, const std::string password,
  const wifi_auth_mode_t authMode) {
  esp_wifi_sta_enterprise_disable();
  esp_wifi_disconnect();

  wifi_config_t wifi_config = {};
  snprintf((char*)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid.c_str());
  snprintf((char*)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", password.c_str());

  wifi_config.sta.threshold.authmode = authMode;
  wifi_config.sta.pmf_cfg.capable = true;
  wifi_config.sta.pmf_cfg.required = false;

  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  return awaitConnected();
}

bool WiFi::attemptConnect(const std::string ssid, const std::string uname,
  const std::string password, const wifi_auth_mode_t authMode) {
  esp_wifi_disconnect();

  // 1. Auto-generate the Identity
  std::string identity = "anonymous";
  size_t atPos = uname.find('@');
  if (atPos != std::string::npos) identity += uname.substr(atPos);
  
  printf("Real User: %s\n", uname.c_str());
  printf("Outer ID : %s (Privacy Safe)\n", identity.c_str());

  wifi_config_t wifi_config = {};
  snprintf((char*)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", ssid.c_str());
  wifi_config.sta.threshold.authmode = authMode;
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

  // 3. Set the calculated identity (using new ESP-IDF v5.x API)
  esp_wifi_sta_enterprise_enable();
  esp_eap_client_set_identity((uint8_t *)identity.c_str(), identity.length());
  esp_eap_client_set_username((uint8_t *)uname.c_str(), uname.length());
  esp_eap_client_set_password((uint8_t *)password.c_str(), password.length());

  return awaitConnected();
}

bool WiFi::awaitConnected() {
  authFailed = false;
  if (esp_wifi_connect() != ESP_OK) return false;

  uint8_t attempts = 0;
  while (!isConnected() && attempts < 20) {
    if (authFailed) {
      printf("SSID/Password was wrong! Aborting connection attempt.\n");
      return false;
    }
    vTaskDelay(500);
    attempts++;
  }
  if (isConnected()) return true;
  return false;
}

// ------------- non-class --------------
void scanAndUpdateSSIDList() {
  printf("Starting WiFi Scan...\n");

  // 1. Start Scan (Blocking Mode = true)
  // In blocking mode, this function waits here until scan is done (~2 seconds)
  wifi_scan_config_t scan_config = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = false
  };
  esp_err_t err = esp_wifi_scan_start(&scan_config, true);
  
  if (err != ESP_OK) {
      printf("Scan failed!\n");
      return;
  }

  // 2. Get the results
  uint16_t ap_count = 0;
  esp_wifi_scan_get_ap_num(&ap_count);
  
  // Limit to 10 networks to save RAM/BLE MTU space
  if (ap_count > 10) ap_count = 10;

  wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
  ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

  // 3. Build JSON using cJSON
  cJSON *root = cJSON_CreateArray();

  for (int i = 0; i < ap_count; i++) {
      cJSON *item = cJSON_CreateObject();
      // ESP-IDF stores SSID as uint8_t, cast to char*
      cJSON_AddStringToObject(item, "ssid", (char *)ap_list[i].ssid);
      cJSON_AddNumberToObject(item, "rssi", ap_list[i].rssi);
      // Add encryption type if you want (optional)
      cJSON_AddNumberToObject(item, "auth", ap_list[i].authmode);
      
      cJSON_AddItemToArray(root, item);
  }

  // 4. Convert to String
  char *json_string = cJSON_PrintUnformatted(root); // Compact JSON
  printf("JSON: %s\n", json_string);

  // 5. Update BLE
  if (ssidListChar != nullptr) {
      ssidListChar->setValue(std::string(json_string));
      ssidListChar->notify();
  }

  // 6. Cleanup Memory (CRITICAL in C++)
  free(ap_list);
  cJSON_Delete(root); // This deletes all children (items) too
  free(json_string);  // cJSON_Print allocates memory, you must free it
}