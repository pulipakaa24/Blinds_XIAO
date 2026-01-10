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
#include "esp_pm.h"

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

  xTaskCreate(setupLoop, "Setup", 8192, NULL, 5, &setupTaskHandle);
  
  // TOMORROW!!!
  // statusResolved = false;
  
  // // Main loop
  // while (1) {
  //   // websocket disconnect/reconnect handling
  //   if (statusResolved) {
  //     if (!connected) {
  //       printf("Disconnected! Beginning setup loop.\n");
  //       stopSocketIO();
  //       setupLoop();
  //     }
  //     else printf("Reconnected!\n");
  //     statusResolved = false;
  //   }

  //   if (clearCalibFlag) {
  //     calib.clearCalibrated();
  //     emitCalibStatus(false);
  //     clearCalibFlag = false;
  //   }
  //   if (savePosFlag) {
  //     servoSavePos();
  //     savePosFlag = false;

  //     // Send position update to server
  //     uint8_t currentAppPos = calib.convertToAppPos(topEnc->getCount());
  //     emitPosHit(currentAppPos);
      
  //     printf("Sent pos_hit: position %d\n", currentAppPos);
  //   }
  //   vTaskDelay(pdMS_TO_TICKS(100));
  // }
}

void pm_init() {
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,        // Max CPU frequency
        .min_freq_mhz = 80,         // Min CPU frequency (DFS)
        .light_sleep_enable = true  // ALLOW CPU to power down during idle
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

extern "C" void app_main() {
  pm_init();
  mainApp();
}