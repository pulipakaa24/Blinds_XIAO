#include "setup.hpp"
#include "BLE.hpp"
#include "WiFi.hpp"
#include "nvs_flash.h"
#include "defines.h"
#include "bmHTTP.hpp"
#include "socketIO.hpp"

TaskHandle_t setupTaskHandle = NULL;

void initialSetup() {
  printf("Entered Setup\n");
  NimBLEAdvertising* pAdv = initBLE();
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void setupLoop(void *pvParameters) {
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
              statusResolved = false;
              initSocketIO();
              
              // Wait for device_init message from server with timeout
              int timeout_count = 0;
              const int MAX_TIMEOUT = 60; // 10 seconds (20 * 500ms)
              while (!statusResolved && timeout_count < MAX_TIMEOUT) {
                printf("Waiting for device_init message... (%d/%d)\n", timeout_count, MAX_TIMEOUT);
                vTaskDelay(pdMS_TO_TICKS(500));
                timeout_count++;
              }
              
              if (timeout_count >= MAX_TIMEOUT) {
                printf("Timeout waiting for device_init - connection failed\n");
                stopSocketIO();
                initSuccess = false;
                initialSetup();
              } else {
                initSuccess = connected;
                if (!initSuccess) {
                  printf("Device authentication failed - entering setup\n");
                  initialSetup();
                }
              }
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
}