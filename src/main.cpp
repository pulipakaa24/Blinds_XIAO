#include <driver/gptimer.h>
#include "servo.hpp"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "WiFi.hpp"
#include "setup.hpp"
#include "socketIO.hpp"
#include "encoder.hpp"
#include "calibration.hpp"

// Global encoder instances
Encoder* topEnc = new Encoder(ENCODER_PIN_A, ENCODER_PIN_B);
Encoder* bottomEnc = new Encoder(InputEnc_PIN_A, InputEnc_PIN_B);

// Global encoder pointers (used by servo.cpp)

// Global calibration instance
Calibration calib;

void mainApp() {
  esp_err_t ret = nvs_flash_init(); // change to secure init logic soon!!
  // 2. If NVS is full or corrupt (common after flashing new code), erase and retry
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  bmWiFi.init();
  calib.init();
  
  // Initialize encoders
  topEnc->init();
  bottomEnc->init();
  servoInit();

  setupLoop();
  
  statusResolved = false;

  int32_t prevCount = topEnc->getCount();
  
  // Main loop
  while (1) {
    // websocket disconnect/reconnect handling
    if (statusResolved) {
      if (!connected) {
        printf("Disconnected! Beginning setup loop.\n");
        stopSocketIO();
        setupLoop();
      }
      else printf("Reconnected!\n");
      statusResolved = false;
    }

    if (clearCalibFlag) {
      calib.clearCalibrated();
      emitCalibStatus(false);
      clearCalibFlag = false;
    }
    if (savePosFlag) {
      servoSavePos();
      savePosFlag = false;

      // Send position update to server
      uint8_t currentAppPos = calib.convertToAppPos(topEnc->getCount());
      emitPosHit(currentAppPos);
      
      printf("Sent pos_hit: position %d\n", currentAppPos);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void encoderTest() {
  // Create encoder instance
  Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);
  encoder.init();

  int32_t prevCount = encoder.getCount();

  while (1) {
    int32_t currentCount = encoder.getCount();
    if (currentCount != prevCount) {
      prevCount = currentCount;
      printf("Encoder Pos: %d\n", prevCount);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

extern "C" void app_main() {
  mainApp();
  // encoderTest();
}