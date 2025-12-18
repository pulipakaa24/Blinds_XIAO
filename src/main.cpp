#include <driver/gptimer.h>
#include "pwm.h"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "BLE.hpp"
#include "WiFi.hpp"
#include "setup.hpp"

extern "C" void app_main() {
  esp_err_t ret = nvs_flash_init(); // change to secure init logic soon!!
  // 2. If NVS is full or corrupt (common after flashing new code), erase and retry
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  bmWiFi.init();
  
  nvs_handle_t WiFiHandle;
  if (nvs_open(nvsWiFi, NVS_READWRITE, &WiFiHandle) == ESP_OK) {
    size_t ssidSize;
    esp_err_t WiFiPrefsError = nvs_get_str(WiFiHandle, ssidTag, NULL, &ssidSize);
    size_t pwSize;
    WiFiPrefsError |= nvs_get_str(WiFiHandle, passTag, NULL, &pwSize);
    uint8_t authMode;
    WiFiPrefsError |= nvs_get_u8(WiFiHandle, authTag, &authMode);

    if (WiFiPrefsError == ESP_ERR_NVS_NOT_FOUND) {
      // Make the RGB LED a certain color (Blue?)
      nvs_close(WiFiHandle);
      initialSetup();
    } else if (WiFiPrefsError == ESP_OK) {
      char ssid[ssidSize];
      nvs_get_str(WiFiHandle, ssidTag, ssid, &ssidSize);
      char pw[pwSize];
      nvs_get_str(WiFiHandle, passTag, pw, &pwSize);
      nvs_close(WiFiHandle);
      // TODO: add enterprise support
      if (!bmWiFi.attemptConnect(ssid, pw, (wifi_auth_mode_t)authMode)) {
        // Make RGB LED certain color (Blue?)
        initialSetup();
      }
    } else {
      // Make RGB LED certain color (Blue?)
      nvs_close(WiFiHandle);
      printf("Program error in Wifi Connection\n");
      initialSetup();
    }
  }
  else {
    printf("ERROR: Couldn't open wifi NVS segment\nProgram stopped.\n");
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  // Main loop
  while (1) {
    // Your main application logic here
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}