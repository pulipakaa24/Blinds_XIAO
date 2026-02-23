#include "max17048.h"
#include "esp_err.h"
#include "i2c.h"
#include "esp_timer.h"

static const char *TAG = "BATTERY";
uint8_t established_soc;

esp_err_t max17048_init() {
  esp_err_t err = ESP_OK;
  uint8_t status, _;
  err |= i2c_init();
  err |= max17048_read_reg(MAX17048_REG_STATUS, &status, &_);
  err |= bms_set_alert_bound_voltages(3.3, 4.2);
  err |= bms_set_reset_voltage(3.25);
  err |= bms_set_alsc();
  err |= bms_clear_status();

}

// Helper: Function reading MAX17048 SOC register, returning battery percentage
uint8_t bms_get_soc() {
  // uint16_t raw_soc, raw_vcell;
  uint16_t raw_soc;

  // Read SOC (Register 0x04)
  if (max17048_read_reg(MAX17048_REG_SOC, ((uint8_t*)&raw_soc)+1, (uint8_t*)&raw_soc) == ESP_OK) {
    return (raw_soc >> 8) + raw_soc & 0x80; // round to the nearest percent
  } else {
    ESP_LOGE(TAG, "Failed to read MAX17048");
    return 0;
  }
}

esp_err_t bms_set_alert_bound_voltages(float min, float max) {
  uint8_t minVal = (uint16_t)((float)min * 1000.0) / 20;
  uint8_t maxVal = (uint16_t)((float)max * 1000.0) / 20;
  return max17048_write_reg(MAX17048_REG_VALRT, minVal, maxVal);
}

esp_err_t bms_set_reset_voltage(float vreset) {
  uint8_t maxVal = (uint16_t)((float)vreset * 1000.0) / 40;
  max17048_write_reg(MAX17048_REG_VRST_ID, vreset, 0);
}