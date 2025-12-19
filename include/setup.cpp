#include "setup.hpp"
#include "BLE.hpp"
#include "WiFi.hpp"

void initialSetup() {
  NimBLEAdvertising* pAdv = initBLE();

  while (!BLEtick(pAdv)) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}