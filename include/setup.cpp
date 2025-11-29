#include "setup.hpp"
#include "BLE.hpp"
#include "WiFi.hpp"

void initialSetup() {
  NimBLEAdvertising* pAdv = initBLE();

  while (1) { // try to connect to wifi too.
    BLEtick(pAdv);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}