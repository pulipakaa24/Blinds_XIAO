#include "freertos/FreeRTOS.h"
#include "setup.hpp"
#include "BLE.hpp"
#include "WiFi.hpp"
#include "nvs_flash.h"
#include "defines.h"
#include "bmHTTP.hpp"
#include "socketIO.hpp"
#include "calibration.hpp"

TaskHandle_t setupTaskHandle = NULL;

std::atomic<bool> awaitCalibration{false};

void initialSetup() {
  printf("Entered Setup\n");
  initBLE();
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  setupTaskHandle = NULL;
}

void setupAndCalibrate() {
  while (1) {
    setupLoop();
    if (awaitCalibration) {
      if (calibrate()) break;
    }
    else break;
  }
}

void setupLoop() {
  setupTaskHandle = xTaskGetCurrentTaskHandle();
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
        continue;
      } 
      else if (WiFiPrefsError == ESP_OK) {
        char ssid[ssidSize];
        nvs_get_str(WiFiHandle, ssidTag, ssid, &ssidSize);
        char pw[pwSize];
        nvs_get_str(WiFiHandle, passTag, pw, &pwSize);
        nvs_close(WiFiHandle);
        if (!WiFi::attemptConnect(ssid, pw, (wifi_auth_mode_t)authMode)) {
          // Make RGB LED certain color (Blue?)
          printf("Found credentials, failed to connect.\n");
          initialSetup();
          continue;
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
              cJSON* response = nullptr;
              initSuccess = httpGET("position", webToken, response);
              if (!initSuccess) {
                initialSetup();
                continue;
              }
              initSuccess = false;

              if (response != NULL) {
                // Check if response is an array
                if (cJSON_IsArray(response)) {
                  int arraySize = cJSON_GetArraySize(response);
                  
                  // Condition 1: More than one object in array
                  if (arraySize > 1) {
                    printf("Multiple peripherals detected, entering setup.\n");
                    cJSON_Delete(response);
                    initialSetup();
                    continue;
                  }
                  // Condition 2: Check peripheral_number in first object
                  else if (arraySize > 0) {
                    cJSON *firstObject = cJSON_GetArrayItem(response, 0);
                    if (firstObject != NULL) {
                      cJSON *peripheralNum = cJSON_GetObjectItem(firstObject, "peripheral_number");
                      if (cJSON_IsNumber(peripheralNum) && peripheralNum->valueint != 1) {
                        printf("Peripheral number is not 1, entering setup.\n");
                        cJSON_Delete(response);
                        initialSetup();
                        continue;
                      }
                      // Valid single peripheral with number 1, continue with normal flow
                      initSuccess = true;
                      printf("Verified new token!\n");

                      cJSON *awaitCalib = cJSON_GetObjectItem(firstObject, "await_calib");
                      if (cJSON_IsBool(awaitCalib)) awaitCalibration = awaitCalib->valueint;
                      cJSON_Delete(response);
                      if (!awaitCalibration) {
                        // Create calibration status object
                        cJSON* calibPostObj = cJSON_CreateObject();
                        cJSON_AddNumberToObject(calibPostObj, "port", 1);
                        cJSON_AddBoolToObject(calibPostObj, "calibrated", Calibration::getCalibrated());
                        
                        // Send calibration status to server
                        cJSON* calibResponse = nullptr;
                        bool calibSuccess = httpPOST("report_calib_status", webToken, calibPostObj, calibResponse);
                        
                        if (calibSuccess && calibResponse != NULL) {
                          printf("Calibration status reported successfully\n");
                          cJSON_Delete(calibResponse);
                        } else {
                          printf("Failed to report calibration status\n");
                        }
                        cJSON_Delete(calibPostObj);
                        if (!Calibration::getCalibrated()) awaitCalibration = true;
                      }
                    }
                    else {
                      printf("null object\n");
                      cJSON_Delete(response);
                      initialSetup();
                      continue;
                    }
                  }
                  else {
                    printf("no items in array\n");
                    cJSON_Delete(response);
                    initialSetup();
                    continue;
                  }
                }
                else {
                  printf("Response not array\n");
                  cJSON_Delete(response);
                  initialSetup();
                  continue;
                }
              } 
              else printf("Failed to parse JSON response\n");
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
  setupTaskHandle = NULL; // Clear handle on function exit (safety)
}