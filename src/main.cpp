#include <driver/gptimer.h>
#include "pwm.h"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "WiFi.hpp"
#include "setup.hpp"
#include "socketIO.hpp"
#include "encoder.hpp"
#include "calibration.hpp"

// Global encoder instances
Encoder topEnc(ENCODER_PIN_A, ENCODER_PIN_B);
Encoder bottomEnc(InputEnc_PIN_A, InputEnc_PIN_B);

// Global calibration instance
Calibration calib(topEnc);

void mainApp() {
  printf("Hello ");
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
  topEnc.init();
  bottomEnc.init();

  setupLoop();
  
  statusResolved = false;

  int32_t prevCount = topEnc.getCount();
  
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

    // Your main application logic here
    int32_t currentCount = topEnc.getCount();
    if (currentCount != prevCount) {
      prevCount = currentCount;
      printf("Encoder Pos: %d\n", prevCount);
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