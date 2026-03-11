#include "bms_test.hpp"
#include "i2c.h"
#include "max17048.h"
#include "defines.h"

void bms_test_LED() {
  vTaskDelay(pdMS_TO_TICKS(5000));
  i2c_init();

  gpio_config_t led_conf = {
    .pin_bit_mask = (1ULL << debugLED),
    .mode         = GPIO_MODE_OUTPUT,
    .pull_up_en   = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
  };
  gpio_config(&led_conf);

  bms_set_alert_bound_voltages(3.8f, 3.9f);
  bms_clear_status();
  bms_clear_alrt();

  while (true) {
    uint8_t msb, lsb;
    if (max17048_read_reg(MAX17048_REG_STATUS, &msb, &lsb) == ESP_OK) {
      bool vh = !!(msb & VHbit);
      bool vl = !!(msb & VLbit);

      bms_clear_status();
      bms_clear_alrt();
      if (vl) {
        // Under-voltage: LED off
        gpio_set_level(debugLED, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
      } else if (vh) {
        // Over-voltage: rapid blink
        gpio_set_level(debugLED, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(debugLED, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
      } else {
        // Normal: LED on
        gpio_set_level(debugLED, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

void bms_test() {
  vTaskDelay(pdMS_TO_TICKS(5000));
  esp_err_t err = i2c_init();

  printf("Error after init: %d\n", err);

  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << maxALRT),
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);

  printf("BMS test running. Polling ALRT pin every 1s...\n");


  uint8_t msb, lsb;
  if (max17048_read_reg(MAX17048_REG_STATUS, &msb, &lsb) == ESP_OK) {
    printf("  STATUS before clear: RI=%d\n", !!(msb & RIbit));
    printf("  STATUS reg before clear: RI=%x, %x\n", msb, lsb);
  } else {
    printf("  STATUS read FAILED\n");
  }

  err = bms_set_alert_bound_voltages(3.3f, 4.2f);
  printf("Error after boundV: %d\n", err);
  err |= bms_clear_status();
  printf("Error after clearStatus: %d\n", err);
  err |= bms_clear_alrt();
  printf("Error after clearALRT: %d\n", err);

  if (max17048_read_reg(MAX17048_REG_STATUS, &msb, &lsb) == ESP_OK) {
    printf("  STATUS after clear: RI=%d\n", !!(msb & RIbit));
    printf("  STATUS reg after clear: RI=%x, %x\n", msb, lsb);
  } else {
    printf("  STATUS read FAILED\n");
  }
  
  while (true) {
    int level = gpio_get_level(maxALRT);
    printf("ALRT pin: %s\n", level ? "HIGH (no alert)" : "LOW (alert asserted)");

    if (level == 0) {
      if (max17048_read_reg(MAX17048_REG_STATUS, &msb, &lsb) == ESP_OK) {
        printf("  STATUS: VH=%d  VL=%d  HD=%d  SC=%d\n",
          !!(msb & VHbit),
          !!(msb & VLbit),
          !!(msb & HDbit),
          !!(msb & SCbit));
        bms_clear_status();
        bms_clear_alrt();
      } else {
        printf("  STATUS read FAILED\n");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}