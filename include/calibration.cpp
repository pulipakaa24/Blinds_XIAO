#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "calibration.hpp"
#include "defines.h"
#include "nvs_flash.h"
#include "socketIO.hpp"
#include <limits.h>

// Static member definitions
std::atomic<bool> Calibration::calibrated{false};
std::atomic<int32_t> Calibration::UpTicks{0};
std::atomic<int32_t> Calibration::DownTicks{0};
TaskHandle_t calibTaskHandle = NULL;

void Calibration::init() {
  nvs_handle_t calibHandle;
  if (nvs_open(nvsCalib, NVS_READONLY, &calibHandle) == ESP_OK) {
    int32_t tempUpTicks;
    int32_t tempDownTicks;
    uint8_t tempCalib;
    esp_err_t err = ESP_OK;
    err |= nvs_get_i32(calibHandle, UpTicksTag, &tempUpTicks);
    err |= nvs_get_i32(calibHandle, DownTicksTag, &tempDownTicks);
    err |= nvs_get_u8(calibHandle, statusTag, &tempCalib);
    if (err == ESP_OK) {
      UpTicks = tempUpTicks;
      DownTicks = tempDownTicks;
      calibrated = tempCalib;
      printf("Range: %d - %d\n", tempUpTicks, tempDownTicks);
    }
    else {
      printf("Data missing from NVS\n");
      calibrated = false;
    }
    nvs_close(calibHandle);
  }
  else {
    printf("CALIBINIT: failed to open NVS - not created?\n");
    calibrated = false;
  }
}

bool Calibration::clearCalibrated() {
  if (!calibrated) return true;
  // clear variable and NVS
  calibrated = false;
  nvs_handle_t calibHandle;
  if (nvs_open(nvsCalib, NVS_READWRITE, &calibHandle) == ESP_OK) {
    if (nvs_set_u8(calibHandle, statusTag, false) != ESP_OK) {
      printf("Error saving calibration status as false.\n");
      return false;
    }
    nvs_commit(calibHandle);
    nvs_close(calibHandle);
  }
  else {
    printf("Error opening calibration NVS segment.\n");
    return false;
  }
  return true;
}

bool Calibration::beginDownwardCalib(Encoder& topEnc) {
  int32_t tempUpTicks = topEnc.getCount();
  nvs_handle_t calibHandle;
  if (nvs_open(nvsCalib, NVS_READWRITE, &calibHandle) == ESP_OK) {
    if (nvs_set_i32(calibHandle, UpTicksTag, tempUpTicks) == ESP_OK) {
      printf("Saved UpTicks to NVS\n");
      UpTicks = tempUpTicks;
      nvs_commit(calibHandle);
    }
    else {
      printf("Error saving UpTicks.\n");
      return false;
    }
    nvs_close(calibHandle);
  }
  else {
    printf("Error opening NVS to save UpTicks\n");
    return false;
  }
  return true;
}

bool Calibration::completeCalib(Encoder& topEnc) {
  int32_t tempDownTicks = topEnc.getCount();
  if (tempDownTicks == UpTicks) {
    printf("ERROR: NO RANGE\n");
    return false;
  }
  nvs_handle_t calibHandle;
  if (nvs_open(nvsCalib, NVS_READWRITE, &calibHandle) == ESP_OK) {
    esp_err_t err = ESP_OK;
    err |= nvs_set_i32(calibHandle, DownTicksTag, tempDownTicks);
    err |= nvs_set_u8(calibHandle, statusTag, true);
    if (err != ESP_OK) {
      printf("Error saving calibration data.\n");
      return false;
    }
    DownTicks = tempDownTicks;
    calibrated = true;
    printf("Range: %d - %d\n", UpTicks.load(), tempDownTicks);
    nvs_commit(calibHandle);
    nvs_close(calibHandle);
  }
  else {
    printf("Error opening calibration NVS segment.\n");
    return false;
  }
  return true;
}

int32_t Calibration::convertToTicks(uint8_t appPos) {
  // appPos between 0 and 10, convert to target encoder ticks.
  return (((int32_t)appPos * (UpTicks - DownTicks)) / 10) + DownTicks;
}

uint8_t Calibration::convertToAppPos(int32_t ticks) {
  // appPos between 0 and 10, convert to target encoder ticks.
  int8_t retVal = (ticks - DownTicks) * 10 / (UpTicks - DownTicks);
  return (retVal < 0) ? 0 : ((retVal > 10) ? 10 : retVal);
}

bool calibrate() {
  calibTaskHandle = xTaskGetCurrentTaskHandle();
  printf("Connecting to Socket.IO server for calibration...\n");
  initSocketIO();
  
  // Wait for device_init message from server with timeout
  int timeout_count = 0;
  const int MAX_TIMEOUT = 60; // seconds

  uint32_t status;
  // Wait for notification with timeout
  if (xTaskNotifyWait(0, ULONG_MAX, &status, pdMS_TO_TICKS(MAX_TIMEOUT * 1000)) == pdTRUE) {
    // Notification received within timeout
    if (status) {
      printf("Connected successfully, awaiting destroy command\n");
      xTaskNotifyWait(0, ULONG_MAX, &status, portMAX_DELAY);
      calibTaskHandle = NULL;
      if (status == 2) { // calibration complete
        printf("Calibration process complete\n");
        stopSocketIO();
        return true;
      }
      else { // unexpected disconnect
        printf("Disconnected unexpectedly!\n");
        stopSocketIO();
        return false;
      }
    } else {
      calibTaskHandle = NULL;
      printf("Connection failed! Returning to setup.\n");
      stopSocketIO();
      return false;
    }
  } else {
    // Timeout reached
    calibTaskHandle = NULL;
    printf("Timeout waiting for device_init - connection failed\n");
    stopSocketIO();
    return false;
  }
}