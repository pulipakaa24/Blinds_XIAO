#include "mainEventLoop.hpp"
#include "calibration.hpp"
#include "setup.hpp"
#include "servo.hpp"
#include "bmHTTP.hpp"
#include "cJSON.h"
#include "encoder.hpp"
#include "WiFi.hpp"
#include "socketIO.hpp"
#include "max17048.h"
#include "esp_sleep.h"
#include "defines.h"

// Give C linkage so max17048.c (a C translation unit) can link against these
extern "C" {
  TaskHandle_t  wakeTaskHandle   = NULL;
  QueueHandle_t main_event_queue = NULL;
}

// ── Battery helpers ───────────────────────────────────────────────────────────

static const char* battAlertTypeStr(batt_alert_type_t type) {
  switch (type) {
    case BATT_ALERT_OVERVOLTAGE:          return "overvoltage";
    case BATT_ALERT_CRITICAL_LOW:         return "critical_low";
    case BATT_ALERT_LOW_VOLTAGE_WARNING:  return "low_voltage_warning";
    case BATT_ALERT_SOC_LOW_20:           return "low_20";
    case BATT_ALERT_SOC_LOW_10:           return "low_10";
    default:                               return "unknown";
  }
}

static bool postBatteryAlert(batt_alert_type_t type, uint8_t soc) {
  cJSON* payload = cJSON_CreateObject();
  cJSON_AddStringToObject(payload, "type", battAlertTypeStr(type));
  cJSON_AddNumberToObject(payload, "soc",  soc);
  cJSON* response = nullptr;
  bool ok = httpPOST("battery_alert", webToken, payload, response);
  cJSON_Delete(payload);
  if (response) cJSON_Delete(response);
  return ok;
}

static bool postBatterySoc(uint8_t soc) {
  cJSON* payload = cJSON_CreateObject();
  cJSON_AddNumberToObject(payload, "soc", soc);
  cJSON* response = nullptr;
  bool ok = httpPOST("battery_update", webToken, payload, response);
  cJSON_Delete(payload);
  if (response) cJSON_Delete(response);
  return ok;
}

// ── Position helpers ──────────────────────────────────────────────────────────

bool postServoPos(uint8_t currentAppPos) {
  cJSON* posData = cJSON_CreateObject();
  cJSON_AddNumberToObject(posData, "port", 1);
  cJSON_AddNumberToObject(posData, "pos",  currentAppPos);

  cJSON* response = nullptr;
  bool success = httpPOST("position", webToken, posData, response);
  cJSON_Delete(posData);

  if (success && response != nullptr) {
    cJSON* awaitCalibItem = cJSON_GetObjectItem(response, "await_calib");
    bool awaitCalib = cJSON_IsBool(awaitCalibItem) && awaitCalibItem->valueint != 0;
    printf("Position update sent: %d, await_calib=%d\n", currentAppPos, awaitCalib);
    cJSON_Delete(response);

    if (awaitCalib) {
      gpio_set_level(debugLED, 1); // Start with LED off
      gpio_hold_en(debugLED);
      Calibration::clearCalibrated();
      if (!calibrate()) {
        if (!WiFi::attemptDHCPrenewal())
          setupAndCalibrate();
        else if (!calibrate()) {
          printf("ERROR: EVEN AFTER DHCP RENEWAL, SOCKET OPENING FAIL\n");
          setupAndCalibrate();
        }
      }
      gpio_hold_dis(debugLED);
      gpio_set_level(debugLED, 0);
    }
  }
  return success;
}

bool getServoPos() {
  cJSON* response = nullptr;
  bool success = httpGET("position", webToken, response);

  if (success && response != NULL) {
    if (cJSON_IsArray(response)) {
      int arraySize = cJSON_GetArraySize(response);

      if (arraySize > 1) {
        printf("Multiple peripherals detected, entering setup.\n");
        cJSON_Delete(response);
        return false;
      } else if (arraySize > 0) {
        cJSON* firstObject = cJSON_GetArrayItem(response, 0);
        if (firstObject != NULL) {
          cJSON* peripheralNum = cJSON_GetObjectItem(firstObject, "peripheral_number");
          if (cJSON_IsNumber(peripheralNum) && peripheralNum->valueint != 1) {
            printf("Peripheral number is not 1, entering setup.\n");
            cJSON_Delete(response);
            return false;
          }
          printf("Verified new token!\n");

          cJSON* awaitCalib = cJSON_GetObjectItem(firstObject, "await_calib");
          if (cJSON_IsBool(awaitCalib)) {
            if (awaitCalib->valueint) {
              Calibration::clearCalibrated();
              if (!calibrate()) {
                if (!WiFi::attemptDHCPrenewal())
                  setupAndCalibrate();
                else if (!calibrate()) {
                  printf("ERROR: EVEN AFTER DHCP RENEWAL, SOCKET OPENING FAIL\n");
                  setupAndCalibrate();
                }
              }
            } else {
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

// ── Wake timer (fires every 60 s) ────────────────────────────────────────────

void wakeTimer(void* pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(60000));
    if (setupTaskHandle != NULL || socketIOactive
        || uxQueueMessagesWaiting(main_event_queue) > 2) continue;

    main_event_type_t evt = EVENT_REQUEST_POS;
    xQueueSend(main_event_queue, &evt, portMAX_DELAY);

    evt = EVENT_REPORT_SOC;
    xQueueSend(main_event_queue, &evt, portMAX_DELAY);
  }
}

// ── Main event loop ───────────────────────────────────────────────────────────

void mainEventLoop() {
  main_event_type_t received_event_type;

  while (true) {
    if (!xQueueReceive(main_event_queue, &received_event_type, portMAX_DELAY)) continue;

    if (received_event_type == EVENT_CLEAR_CALIB) {
      Calibration::clearCalibrated();
      if (!calibrate()) {
        if (!WiFi::attemptDHCPrenewal())
          setupAndCalibrate();
        else if (!calibrate()) {
          printf("ERROR: EVEN AFTER DHCP RENEWAL, SOCKET OPENING FAIL\n");
          setupAndCalibrate();
        }
      }

    } else if (received_event_type == EVENT_SAVE_POS) {
      servoSavePos();
      uint8_t currentAppPos = Calibration::convertToAppPos(topEnc->getCount());
      if (!postServoPos(currentAppPos)) {
        printf("Failed to send position update\n");
        if (!WiFi::attemptDHCPrenewal()) {
          setupAndCalibrate();
          postServoPos(currentAppPos);
        } else if (!postServoPos(currentAppPos)) {
          printf("Renewed DHCP but still failed to post position\n");
          setupAndCalibrate();
        }
      }

    } else if (received_event_type == EVENT_REQUEST_POS) {
      if (!getServoPos()) {
        printf("Failed to get position\n");
        if (!WiFi::attemptDHCPrenewal()) {
          setupAndCalibrate();
          getServoPos();
        } else if (!getServoPos()) {
          printf("Renewed DHCP but still failed to get position\n");
          setupAndCalibrate();
        }
      }

    } else if (received_event_type == EVENT_BATTERY_CRITICAL) {
      // Stop the motor immediately, persist position, notify server, then hibernate.
      // esp_deep_sleep_start() with no wakeup source = indefinite sleep until reset.
      // The MAX17048 VRESET comparator handles detection of battery recovery.
      servoOff();
      servoSavePos();
      batt_alert_type_t alertType = bms_pending_alert; // snapshot volatile
      postBatteryAlert(alertType, established_soc);
      printf("CRITICAL BATTERY EVENT (%s, SOC=%d%%). Entering deep sleep.\n",
             battAlertTypeStr(alertType), established_soc);
      esp_deep_sleep_start();

    } else if (received_event_type == EVENT_BATTERY_WARNING) {
      postBatteryAlert((batt_alert_type_t)bms_pending_alert, established_soc);

    } else if (received_event_type == EVENT_REPORT_SOC) {
      postBatterySoc(established_soc);
    }
  }

  vTaskDelete(NULL);
}
