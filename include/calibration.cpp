#include "calibration.hpp"
#include "defines.h"
#include "nvs_flash.h"

void Calibration::init() {
  nvs_handle_t calibHandle;
  if (nvs_open(nvsCalib, NVS_READONLY, &calibHandle) == ESP_OK) {
    int32_t tempUpTicks;
    int32_t tempDownTicks;
    uint8_t tempCalib;
    esp_err_t err = ESP_OK;
    err |= nvs_get_i32(calibHandle, UpTicksTag, &tempUpTicks);
    err |= nvs_get_i32(calibHandle, DownTicksTag, &tempUpTicks);
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