#include "i2c.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "max17048.h"

// Helper: Initializes I2C controller at 100kHz, returns 0=success
esp_err_t i2c_init() {
  // 1. Initialize I2C
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = I2C_MASTER_SDA_IO,
    .scl_io_num = I2C_MASTER_SCL_IO,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = I2C_MASTER_FREQ_HZ,
  };

  return (i2c_param_config(I2C_MASTER_NUM, &conf) || 
  i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}
