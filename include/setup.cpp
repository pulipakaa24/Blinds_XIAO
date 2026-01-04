#include "setup.hpp"
#include "BLE.hpp"
#include "WiFi.hpp"
#include "nvs_flash.h"
#include "defines.h"
#include "bmHTTP.hpp"
#include "socketIO.hpp"
#include <mutex>

// External declarations from BLE.cpp
extern std::mutex dataMutex;
extern wifi_auth_mode_t auth;
extern std::string SSID;
extern std::string PASS;
extern std::string UNAME;
extern std::string TOKEN;
extern std::atomic<bool> scanBlock;
extern std::atomic<bool> finalAuth;

// External functions from BLE.cpp
extern void notifyConnectionStatus(bool success);
extern void notifyAuthStatus(bool success);

// BLE setup task - event-driven instead of polling
void bleSetupTask(void* arg) {
  NimBLEAdvertising* pAdvertising = initBLE();
  EventBits_t bits;
  
  printf("BLE setup task started\n");
  
  while (1) {
    // Wait for BLE events instead of polling
    bits = xEventGroupWaitBits(
        g_system_events,
        EVENT_BLE_SCAN_REQUEST | EVENT_BLE_CREDS_RECEIVED | EVENT_BLE_TOKEN_RECEIVED,
        pdTRUE,   // Clear bits on exit
        pdFALSE,  // Wait for ANY bit (not all)
        portMAX_DELAY  // Block forever until event
    );
    
    if (bits & EVENT_BLE_SCAN_REQUEST) {
      if (!scanBlock) {
        scanBlock = true;
        printf("Scanning WiFi...\n");
        bmWiFi.scanAndUpdateSSIDList();
      }
      else printf("Duplicate scan request\n");
    }
    
    if (bits & EVENT_BLE_CREDS_RECEIVED) {
      std::string tmpSSID;
      std::string tmpUNAME;
      std::string tmpPASS;
      wifi_auth_mode_t tmpAUTH;
      {
        std::lock_guard<std::mutex> lock(dataMutex);
        tmpSSID = SSID;
        tmpUNAME = UNAME;
        tmpPASS = PASS;
        tmpAUTH = auth;
      }

      bool wifiConnect;
      if (tmpAUTH == WIFI_AUTH_WPA2_ENTERPRISE || tmpAUTH == WIFI_AUTH_WPA3_ENTERPRISE)
        wifiConnect = bmWiFi.attemptConnect(tmpSSID, tmpUNAME, tmpPASS, tmpAUTH);
      else wifiConnect = bmWiFi.attemptConnect(tmpSSID, tmpPASS, tmpAUTH);
      
      if (!wifiConnect) {
        notifyConnectionStatus(false);
        printf("Connection failed\n");
        continue;
      }

      nvs_handle_t WiFiHandle;
      esp_err_t err = nvs_open(nvsWiFi, NVS_READWRITE, &WiFiHandle);
      if (err == ESP_OK) {
        esp_err_t saveErr = ESP_OK;
        saveErr |= nvs_set_str(WiFiHandle, ssidTag, tmpSSID.c_str());
        saveErr |= nvs_set_str(WiFiHandle, passTag, tmpPASS.c_str());
        saveErr |= nvs_set_u8(WiFiHandle, authTag, (uint8_t)tmpAUTH);
        
        if (tmpUNAME.length() > 0) {
          saveErr |= nvs_set_str(WiFiHandle, unameTag, tmpUNAME.c_str());
        }
        
        if (saveErr == ESP_OK) {
          nvs_commit(WiFiHandle);
          printf("WiFi credentials saved to NVS\n");
          notifyConnectionStatus(true);
        } else {
          printf("Failed to save WiFi credentials\n");
          notifyConnectionStatus(false);
        }
        nvs_close(WiFiHandle);
      }
    }
    
    if (bits & EVENT_BLE_TOKEN_RECEIVED) {
      std::string tmpTOKEN;
      {
        std::lock_guard<std::mutex> lock(dataMutex);
        tmpTOKEN = TOKEN;
      }

      cJSON* JSONresponse = NULL;
      if (httpGET("api/devices/auth", tmpTOKEN, JSONresponse)) {
        cJSON *success = cJSON_GetObjectItem(JSONresponse, "success");
        bool authSuccess = cJSON_IsTrue(success);
        
        if (authSuccess) {
          nvs_handle_t authHandle;
          if (nvs_open(nvsAuth, NVS_READWRITE, &authHandle) == ESP_OK) {
            if (nvs_set_str(authHandle, tokenTag, tmpTOKEN.c_str()) == ESP_OK) {
              nvs_commit(authHandle);
              printf("Token saved to NVS\n");
              notifyAuthStatus(true);
              finalAuth = true;
              
              // Task complete - delete itself
              vTaskDelete(NULL);
            }
            nvs_close(authHandle);
          }
        } else {
          printf("Token authentication failed\n");
          notifyAuthStatus(false);
        }
        cJSON_Delete(JSONresponse);
      } else {
        printf("HTTP request failed\n");
        notifyAuthStatus(false);
      }
    }
  }
}

void initialSetup() {
  printf("Entered Setup\n");
  
  // Create BLE setup task instead of polling loop
  xTaskCreate(bleSetupTask, "ble_setup", 8192, NULL, BLE_TASK_PRIORITY, NULL);
  
  // Wait for BLE setup to complete (finalAuth = true)
  while (!finalAuth) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  
  printf("BLE setup complete\n");
}

void setupLoop() {
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
              initSocketIO();
              
              // Wait for connection event with timeout
              EventBits_t bits = xEventGroupWaitBits(
                  g_system_events,
                  EVENT_SOCKETIO_CONNECTED | EVENT_SOCKETIO_DISCONNECTED,
                  pdTRUE,  // Clear on exit
                  pdFALSE, // Wait for ANY bit
                  pdMS_TO_TICKS(10000)  // 10 second timeout
              );
              
              if (bits & EVENT_SOCKETIO_CONNECTED) {
                initSuccess = connected;
                if (!initSuccess) {
                  printf("Device authentication failed - entering setup\n");
                  initialSetup();
                }
              } else {
                printf("Timeout waiting for connection\n");
                stopSocketIO();
                initSuccess = false;
                initialSetup();
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