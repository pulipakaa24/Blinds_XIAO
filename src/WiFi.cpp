#include "WiFi.hpp"
#include "esp_eap_client.h"
#include "cJSON.h" // Native replacement for ArduinoJson
#include "BLE.hpp"
#include "esp_wifi_he.h"

TaskHandle_t WiFi::awaitConnectHandle = NULL;
EventGroupHandle_t WiFi::s_wifi_event_group = NULL;
esp_netif_t* WiFi::netif = NULL;
esp_event_handler_instance_t WiFi::instance_any_id = NULL;
esp_event_handler_instance_t WiFi::instance_got_ip = NULL;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_STARTED_BIT BIT1

// The Event Handler (The engine room)
void WiFi::event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
  
  // WiFi driver has finished initialization
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    printf("WiFi initialized and ready\n");
    xEventGroupSetBits(s_wifi_event_group, WIFI_STARTED_BIT);
  }
  // We got disconnected -> Retry automatically
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    // 1. Cast the data to the correct struct
    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
    
    printf("WiFi Disconnected. Reason Code: %d\n", event->reason);

    // 2. Check specific Reason Codes
    switch (event->reason) {
      case WIFI_REASON_AUTH_EXPIRE:               // Reason 2
      case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:    // Reason 15 (Most Common for Wrong Pass)
      case WIFI_REASON_BEACON_TIMEOUT:            // Reason 200
      case WIFI_REASON_AUTH_FAIL:                 // Reason 202
      case WIFI_REASON_HANDSHAKE_TIMEOUT:         // Reason 204
        printf("ERROR: Likely Wrong Password!\n");
        if (awaitConnectHandle != NULL) {
          xTaskNotify(awaitConnectHandle, false, eSetValueWithOverwrite);
        }
        break;
          
      case WIFI_REASON_NO_AP_FOUND:               // Reason 201
        printf("ERROR: SSID Not Found\n");
        if (awaitConnectHandle != NULL) {
          xTaskNotify(awaitConnectHandle, false, eSetValueWithOverwrite);
        }
        break;
      
      case WIFI_REASON_ASSOC_LEAVE:               // Reason 8 - Manual disconnect
        printf("Manual disconnect, not retrying\n");
        break;
          
      case WIFI_REASON_ASSOC_FAIL:                // Reason 203 (Can be AP busy/rate limiting)
        printf("Association failed, will retry...\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second before retry to avoid rate limiting
        esp_netif_dhcpc_start(netif);
        esp_wifi_connect();
        break;
          
      default:
        printf("Retrying...\n");
        esp_netif_dhcpc_start(netif);
        esp_wifi_connect();
        break;
    }
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  } 
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        // This is triggered when the scan finishes!
        printf("Scan complete, processing results...\n");
        
        // Call a function to process results and notify BLE
        processScanResults();
    }
  // 3. We got an IP Address -> Success!
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    printf("Got IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    esp_netif_dhcpc_stop(netif);
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    if (awaitConnectHandle != NULL) {
      xTaskNotify(awaitConnectHandle, true, eSetValueWithOverwrite);
    }
  }
}

void WiFi::init() {
  s_wifi_event_group = xEventGroupCreate();
  // 1. Init Network Interface
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  netif = esp_netif_create_default_wifi_sta();

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
  xEventGroupWaitBits(s_wifi_event_group, WIFI_STARTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
  awaitConnectHandle = NULL;
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
  awaitConnectHandle = xTaskGetCurrentTaskHandle();
  if (esp_wifi_connect() != ESP_OK) {
    awaitConnectHandle = NULL;
    return false;
  }

  uint32_t status;
  uint8_t MAX_TIMEOUT = 10; //seconds
  if (xTaskNotifyWait(0, ULONG_MAX, &status, pdMS_TO_TICKS(MAX_TIMEOUT * 1000)) == pdTRUE) {
    awaitConnectHandle = NULL;
    if (!status) {
      printf("SSID/Password was wrong! Aborting connection attempt.\n");
      return false;
    }
  } else {
    // Timeout - check if connected anyway
    awaitConnectHandle = NULL;
  }
  
  if (isConnected()) {
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    uint8_t protocol_bitmap = 0;
    esp_err_t err = esp_wifi_get_protocol(WIFI_IF_STA, &protocol_bitmap);

    if (err == ESP_OK && (protocol_bitmap & WIFI_PROTOCOL_11AX)) {
      // WiFi 6 (802.11ax) - Use Target Wake Time (TWT) for power saving
      wifi_twt_setup_config_t twt_config = {
        .setup_cmd = TWT_REQUEST,
        .trigger = true,
        .flow_type = 0, // Announced
        .flow_id = 0,
        .wake_invl_expn = 12, // Exponent for interval
        .min_wake_dura = 255, // ~65ms (unit is 256 microseconds)
        .wake_invl_mant = 14648, // Mantissa (mant * 2^exp = 60,000,000 us = 60s)
        .timeout_time_ms = 5000,
      };
      esp_wifi_sta_itwt_setup(&twt_config);
    }
    return true;
  }
  return false;
}

// ------------- non-class --------------
void WiFi::scanAndUpdateSSIDList() {
  printf("Starting WiFi Scan...\n");

  esp_wifi_sta_enterprise_disable();
  esp_wifi_disconnect();

  wifi_scan_config_t scan_config = {
      .ssid = NULL,
      .bssid = NULL,
      .channel = 0,
      .show_hidden = false
  };
  esp_err_t err = esp_wifi_scan_start(&scan_config, false);
  
  if (err != ESP_OK) {
      printf("Scan failed!\n");
      return;
  }
}

void WiFi::processScanResults() {
  // 2. Get the results
  uint16_t ap_count = 0;
  esp_wifi_scan_get_ap_num(&ap_count);
  
  // Limit to 10 networks to save RAM/BLE MTU space
  if (ap_count > 10) ap_count = 10;

  wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
  if (ap_list == NULL) {
    printf("Heap allocation error in processScanResults\n");
    return;
  }
  
  esp_err_t err = esp_wifi_scan_get_ap_records(&ap_count, ap_list);
  if (err != ESP_OK) {
    printf("Failed to get scan records\n");
    free(ap_list);
    return;
  }

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
    ssidListChar.load()->setValue(std::string(json_string));
    NimBLECharacteristic *tmpRefreshChar = ssidRefreshChar.load();
    tmpRefreshChar->setValue("Ready");
    tmpRefreshChar->notify();
  }

  // 6. Cleanup Memory
  free(ap_list);
  cJSON_Delete(root); // This deletes all children (items) too
  free(json_string);  // cJSON_Print allocates memory, you must free it
}

bool WiFi::attemptDHCPrenewal() {
  xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  esp_netif_dhcpc_start(netif);
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, 
                                        WIFI_CONNECTED_BIT, 
                                        pdFALSE, 
                                        pdFALSE, 
                                        pdMS_TO_TICKS(4000));
  
  if (bits & WIFI_CONNECTED_BIT) {
    printf("renewal success");
    // Stop the client again to save power.
    esp_netif_dhcpc_stop(netif);
    return true;
  }
  printf("DHCP Renewal failed. Reconnecting Wi-Fi...\n");
  esp_wifi_disconnect();
  return awaitConnected();
}