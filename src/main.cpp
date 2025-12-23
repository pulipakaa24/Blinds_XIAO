#include <driver/gptimer.h>
#include "pwm.h"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "BLE.hpp"
#include "WiFi.hpp"
#include "setup.hpp"
#include "bmHTTP.hpp"
#include "socketIO.hpp"
#include "cJSON.h"

extern "C" void app_main() {
  printf("Hello ");
  esp_err_t ret = nvs_flash_init(); // change to secure init logic soon!!
  // 2. If NVS is full or corrupt (common after flashing new code), erase and retry
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  bmWiFi.init();

  bool initSuccess = false;
  
  while(!initSuccess) {
    nvs_handle_t WiFiHandle;
    if (nvs_open(nvsWiFi, NVS_READONLY, &WiFiHandle) == ESP_OK) {
      size_t ssidSize;
      esp_err_t WiFiPrefsError = nvs_get_str(WiFiHandle, ssidTag, NULL, &ssidSize);
      size_t pwSize;
      WiFiPrefsError |= nvs_get_str(WiFiHandle, passTag, NULL, &pwSize);
      uint8_t authMode;
      WiFiPrefsError |= nvs_get_u8(WiFiHandle, authTag, &authMode);
      printf("World\n");
      if (WiFiPrefsError == ESP_ERR_NVS_NOT_FOUND) {
        printf("Didn't find creds\n");
        // Make the RGB LED a certain color (Blue?)
        nvs_close(WiFiHandle);
        initialSetup();
      } else if (WiFiPrefsError == ESP_OK) {
        char ssid[ssidSize];
        nvs_get_str(WiFiHandle, ssidTag, ssid, &ssidSize);
        char pw[pwSize];
        nvs_get_str(WiFiHandle, passTag, pw, &pwSize);
        nvs_close(WiFiHandle);
        if (!bmWiFi.attemptConnect(ssid, pw, (wifi_auth_mode_t)authMode)) {
          // Make RGB LED certain color (Blue?)
          printf("Found credentials, failed to connect.\n");
          initialSetup();
        }
        else {
          printf("Connected to WiFi from NVS credentials\n");
          nvs_handle_t authHandle;
          if (nvs_open(nvsAuth, NVS_READONLY, &authHandle) == ESP_OK) {
            size_t tokenSize;
            if (nvs_get_str(authHandle, tokenTag, NULL, &tokenSize) == ESP_OK) {
              char token[tokenSize];
              nvs_get_str(authHandle, tokenTag, token, &tokenSize);
              nvs_close(authHandle);
              
              // Use permanent device token to connect to Socket.IO
              // The server will verify the token during connection handshake
              webToken = std::string(token);
              printf("Connecting to Socket.IO server with saved token...\n");
              initSocketIO();
              while (!statusResolved) vTaskDelay(pdMS_TO_TICKS(100));
              initSuccess = connected;
            }
            else {
              printf("Token read unsuccessful, entering setup.\n");
              initialSetup();
            }
          }
          else {
            printf("Auth NVS segment doesn't exist, entering setup.\n");
            initialSetup();
          }
        }
      } else {
        // Make RGB LED certain color (Blue?)
        nvs_close(WiFiHandle);
        printf("Program error in Wifi Connection\n");
        initialSetup();
      }
    }
    else {
      printf("WiFi NVS segment doesn't exist, entering setup.\n");
      initialSetup();
    }
  }
  

  // Main loop
  while (1) {
    // Your main application logic here
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("Main Loop\n");
  }
}