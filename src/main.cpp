#include <driver/gptimer.h>
#include "pwm.h"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "BLE.hpp"
#include "WiFi.hpp"

extern "C" void app_main() {
  esp_err_t ret = nvs_flash_init();
  // 2. If NVS is full or corrupt (common after flashing new code), erase and retry
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  init_wifi();
  NimBLEAdvertising* pAdv = initBLE();

  while (1) {
    BLEtick(pAdv);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}