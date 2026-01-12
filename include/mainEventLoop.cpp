#include "mainEventLoop.hpp"
#include "calibration.hpp"
#include "setup.hpp"
#include "servo.hpp"
#include "bmHTTP.hpp"
#include "cJSON.h"
#include "encoder.hpp"
#include "WiFi.hpp"

TaskHandle_t wakeTaskHandle = NULL;

void wakeTimer(void* pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(60000));
    main_event_type_t evt = EVENT_REQUEST_POS;
    xQueueSend(main_event_queue, &evt, portMAX_DELAY);
  }
}

bool postServoPos(uint8_t currentAppPos) {
  // Create POST data
  cJSON* posData = cJSON_CreateObject();
  cJSON_AddNumberToObject(posData, "port", 1);
  cJSON_AddNumberToObject(posData, "pos", currentAppPos);
  
  // Send position update
  cJSON* response = nullptr;
  bool success = httpPOST("position", webToken, posData, response);
  cJSON_Delete(posData);
  
  if (success && response != nullptr) {
    // Parse await_calib from response
    cJSON* awaitCalibItem = cJSON_GetObjectItem(response, "await_calib");
    bool awaitCalib = false;
    if (cJSON_IsBool(awaitCalibItem)) {
      awaitCalib = awaitCalibItem->valueint != 0;
    }
    
    printf("Position update sent: %d, await_calib=%d\n", currentAppPos, awaitCalib);
    cJSON_Delete(response);
    
    if (awaitCalib) {
      Calibration::clearCalibrated();
      if (!calibrate()) {
        if (!WiFi::attemptDHCPrenewal())
          setupAndCalibrate();
        else {
          if (!calibrate()) {
            printf("ERROR OCCURED: EVEN AFTER SETUP, SOCKET OPENING FAIL\n");
            setupAndCalibrate();
          }
        }
      }
    }
  }
  return success;
}

bool getServoPos() {
  cJSON* response = nullptr;
  bool success = httpGET("position", webToken, response);

  if (success && response != NULL) {
    // Check if response is an array
    if (cJSON_IsArray(response)) {
      int arraySize = cJSON_GetArraySize(response);
      
      // Condition 1: More than one object in array
      if (arraySize > 1) {
        printf("Multiple peripherals detected, entering setup.\n");
        cJSON_Delete(response);
        return false;
      }
      // Condition 2: Check peripheral_number in first object
      else if (arraySize > 0) {
        cJSON *firstObject = cJSON_GetArrayItem(response, 0);
        if (firstObject != NULL) {
          cJSON *peripheralNum = cJSON_GetObjectItem(firstObject, "peripheral_number");
          if (cJSON_IsNumber(peripheralNum) && peripheralNum->valueint != 1) {
            printf("Peripheral number is not 1, entering setup.\n");
            cJSON_Delete(response);
            return false;
          }
          printf("Verified new token!\n");

          cJSON *awaitCalib = cJSON_GetObjectItem(firstObject, "await_calib");
          if (cJSON_IsBool(awaitCalib)) {
            if (awaitCalib->valueint) {
              Calibration::clearCalibrated();
              if (!calibrate()) {
                if (!WiFi::attemptDHCPrenewal())
                  setupAndCalibrate();
                else {
                  if (!calibrate()) {
                    printf("ERROR OCCURED: EVEN AFTER SETUP, SOCKET OPENING FAIL\n");
                    setupAndCalibrate();
                  }
                }
              }
            }
            else {
              cJSON* pos = cJSON_GetObjectItem(firstObject, "last_pos");
              runToAppPos(pos->valueint);
            }
          }
          cJSON_Delete(response);
        }
      }
    }
  }
  return success;
}

QueueHandle_t main_event_queue = NULL;

void mainEventLoop() {
  main_event_queue = xQueueCreate(10, sizeof(main_event_type_t));
  main_event_type_t received_event_type;

  while (true) {
    if (xQueueReceive(main_event_queue, &received_event_type, portMAX_DELAY)) {
      if (received_event_type == EVENT_CLEAR_CALIB) {
        Calibration::clearCalibrated();
        if (!calibrate()) {
          if (!WiFi::attemptDHCPrenewal())
            setupAndCalibrate();
          else {
            if (!calibrate()) {
              printf("ERROR OCCURED: EVEN AFTER SETUP, SOCKET OPENING FAIL\n");
              setupAndCalibrate();
            }
          }
        }
      }
      else if (received_event_type == EVENT_SAVE_POS) {
        servoSavePos();

        uint8_t currentAppPos = Calibration::convertToAppPos(topEnc->getCount());
        
        if (!postServoPos(currentAppPos)) {
          printf("Failed to send position update\n");
          if (!WiFi::attemptDHCPrenewal()) {
            setupAndCalibrate();
            postServoPos(currentAppPos);
          }
          else {
            if (!postServoPos(currentAppPos)) {
              printf("renewed dhcp successfully, but still failed to post\n");
              setupAndCalibrate();
            }
          }
        }
      }
      else if (received_event_type == EVENT_REQUEST_POS) {
        if (!getServoPos()) {
          printf("Failed to send position update\n");
          if (!WiFi::attemptDHCPrenewal()) {
            setupAndCalibrate();
            getServoPos();
          }
          else {
            if (!getServoPos()) {
              printf("renewed dhcp successfully, but still failed to post\n");
              setupAndCalibrate();
            }
          }
        }
      }
    }
  }
  vTaskDelete(NULL);
}