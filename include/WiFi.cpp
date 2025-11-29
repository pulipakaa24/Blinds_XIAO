#include "WiFi.hpp"
#include "esp_wifi.h"
#include "esp_event.h"
#include "cJSON.h" // Native replacement for ArduinoJson
#include "BLE.hpp"

void init_wifi() {
    // 1. Init Network Interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 2. Init WiFi Driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Set Mode to Station (Client)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

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