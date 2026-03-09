#include "max17048.h"
#include "mainEventLoop.hpp"
#include "i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "BATTERY";

uint8_t established_soc = 100;
volatile batt_alert_type_t bms_pending_alert = BATT_ALERT_CRITICAL_LOW;

static TaskHandle_t bms_event_handler = NULL;

void IRAM_ATTR alrt_ISR(void* arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(bms_event_handler, &xHigherPriorityTaskWoken);
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

esp_err_t max17048_init() {
  esp_err_t err = ESP_OK;
  err |= i2c_init();
  err |= bms_set_alert_bound_voltages(3.3f, 4.2f);
  err |= bms_set_reset_voltage(3.25f);
  err |= bms_set_alsc();

  xTaskCreate(bms_checker_task, "BMS", 4096, NULL, 20, &bms_event_handler);

  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << maxALRT),
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_NEGEDGE,
  };

  gpio_config(&io_conf);
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
  gpio_isr_handler_add(maxALRT, alrt_ISR, NULL);

  ESP_LOGI(TAG, "MAX17048 initialized, ALRT interrupt on GPIO %d", maxALRT);

  err |= bms_clear_status();
  err |= bms_clear_alrt();
  return err;
}

uint8_t bms_get_soc() {
  uint16_t raw_soc;
  if (max17048_read_reg(MAX17048_REG_SOC, ((uint8_t*)&raw_soc) + 1, (uint8_t*)&raw_soc) == ESP_OK) {
    // upper byte = whole percent; lower byte msb = 0.5%; round to nearest
    return (uint8_t)(raw_soc >> 8) + ((raw_soc & 0x80) ? 1 : 0);
  }
  ESP_LOGE(TAG, "Failed to read SOC register");
  return 0;
}

esp_err_t bms_set_alert_bound_voltages(float min, float max) {
  uint8_t minVal = (uint8_t)((uint16_t)(min * 1000.0f) / 20);
  uint8_t maxVal = (uint8_t)((uint16_t)(max * 1000.0f) / 20);
  return max17048_write_reg(MAX17048_REG_VALRT, minVal, maxVal);
}

esp_err_t bms_set_reset_voltage(float vreset) {
  uint8_t val = (uint8_t)((uint16_t)(vreset * 1000.0f) / 40);
  return max17048_write_reg(MAX17048_REG_VRST_ID, val, 0);
}

void bms_checker_task(void *pvParameters) {
  uint8_t prev_soc = 100;

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint8_t status, _;
    if (max17048_read_reg(MAX17048_REG_STATUS, &status, &_) != ESP_OK) {
      ESP_LOGE(TAG, "STATUS read failed");
      bms_clear_alrt();
      continue;
    }

    uint8_t soc = bms_get_soc();
    established_soc = soc;

    // Clear before posting to queue so ALRT pin is deasserted promptly
    bms_clear_status();
    bms_clear_alrt();

    main_event_type_t evt;

    if ((status & VHbit) || (status & HDbit) || ((status & VLbit) && soc < SOC_CRITICAL_VL)) {
      // Critical: overvoltage (hardware fault) or battery truly empty
      bms_pending_alert = (status & VHbit) ? BATT_ALERT_OVERVOLTAGE : BATT_ALERT_CRITICAL_LOW;
      evt = EVENT_BATTERY_CRITICAL;
      xQueueSend(main_event_queue, &evt, portMAX_DELAY);

    } else if (status & VLbit) {
      // Undervoltage but SOC still healthy — likely a transient load spike
      bms_pending_alert = BATT_ALERT_LOW_VOLTAGE_WARNING;
      evt = EVENT_BATTERY_WARNING;
      xQueueSend(main_event_queue, &evt, portMAX_DELAY);

    } else if (status & SCbit) {
      // 1% SOC change: check downward threshold crossings for user notifications
      if (soc <= SOC_WARN_10 && prev_soc > SOC_WARN_10) {
        bms_pending_alert = BATT_ALERT_SOC_LOW_10;
        evt = EVENT_BATTERY_WARNING;
        xQueueSend(main_event_queue, &evt, portMAX_DELAY);
      } else if (soc <= SOC_WARN_20 && prev_soc > SOC_WARN_20) {
        bms_pending_alert = BATT_ALERT_SOC_LOW_20;
        evt = EVENT_BATTERY_WARNING;
        xQueueSend(main_event_queue, &evt, portMAX_DELAY);
      }
      prev_soc = soc;
    }
  }
}
