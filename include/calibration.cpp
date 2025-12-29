#include "calibration.hpp"
#include "defines.h"
#include "nvs_flash.h"

void Calibration::init() {
  nvs_handle_t calibHandle;
  if (nvs_open(nvsCalib, NVS_READONLY, &calibHandle) == ESP_OK) {
    int32_t tempTicks;
    if (nvs_get_i32(calibHandle, UpMinusDownTicksTag, &tempTicks) == ESP_OK) {
      uint8_t tempCalib;
      if (nvs_get_u8(calibHandle, statusTag, &tempCalib) == ESP_OK) {
        UpMinusDownTicks = tempTicks;
        calibrated = tempCalib;
        printf("Range: %d\n", tempTicks);
      }
      else {
        printf("No status present\n");
        calibrated = false;
      }
    }
    else {
      printf("No Updownticks present\n");
      calibrated = false;
    }
    nvs_close(calibHandle);
  }
  else {
    printf("CALIBINIT: failed to open NVS - not created?\n");
    calibrated = false;
  }
}

void Calibration::clearCalibrated() {
  if (!calibrated) return;
  
  // clear variable and NVS
  calibrated = false;
  nvs_handle_t calibHandle;
  if (nvs_open(nvsCalib, NVS_READWRITE, &calibHandle) == ESP_OK) {
    if (nvs_set_u8(calibHandle, statusTag, false) != ESP_OK)
      printf("Error saving calibration status as false.\n");
    nvs_close(calibHandle);
  }
  else printf("Error opening calibration NVS segment.\n");
}

bool Calibration::completeCalib() {
  int32_t tempUpMinusDownTicks = startTicks - topEnc.getCount();
  if (calibrated && UpMinusDownTicks == tempUpMinusDownTicks) return true;
  else {
    nvs_handle_t calibHandle;
    if (nvs_open(nvsCalib, NVS_READWRITE, &calibHandle) == ESP_OK) {
      esp_err_t err = ESP_OK;
      if (UpMinusDownTicks != tempUpMinusDownTicks)
        err |= nvs_set_i32(calibHandle, UpMinusDownTicksTag, tempUpMinusDownTicks);
      if (!calibrated)
        err |= nvs_set_u8(calibHandle, statusTag, true);
      if (err != ESP_OK) {
        printf("Error saving calibration data.\n");
        return false;
      }
      UpMinusDownTicks = tempUpMinusDownTicks;
      calibrated = true;
      printf("Range: %d\n", tempUpMinusDownTicks);
      nvs_close(calibHandle);
    }
    else {
      printf("Error opening calibration NVS segment.\n");
      return false;
    }
  }
  return true;
}

int32_t Calibration::convertToTicks(int8_t steps10) {
  // steps10 between -10 and +10
  // with +10 meaning full length upward, -10 meaning full length downward.
  return ((int32_t)steps10 * UpMinusDownTicks) / 10;
}