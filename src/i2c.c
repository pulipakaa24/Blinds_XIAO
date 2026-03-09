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

// Helper: Read 16-bit register to 2-byte array (MSB first big endian)
esp_err_t max17048_read_reg(uint8_t reg_addr, uint8_t *MSB, uint8_t *LSB) {  
  // this is better than converting to little endian for my application
  // since I usually need to handle bytes individually.
  uint8_t data[2];
  // Write register address
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (MAX17048_ADDR << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg_addr, true);
  
  // Restart and Read 2 bytes
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (MAX17048_ADDR << 1) | I2C_MASTER_READ, true);
  i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
  i2c_master_stop(cmd);
  
  esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
  i2c_cmd_link_delete(cmd);

  if (ret == ESP_OK) {
    *MSB = data[0];
    *LSB = data[1];
  }

  return ret;
}

// Write big endian 2-byte array to a 16-bit register
esp_err_t max17048_write_reg(uint8_t reg_addr, uint8_t MSB, uint8_t LSB) {
  // Write register address
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (MAX17048_ADDR << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg_addr, true);
  i2c_master_write_byte(cmd, MSB, true);
  i2c_master_write_byte(cmd, LSB, true);
  i2c_master_stop(cmd);
  
  esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
  i2c_cmd_link_delete(cmd);
  return ret;
}

esp_err_t max17048_friendly_write_reg(uint8_t reg_addr, uint8_t MSB, uint8_t LSB,
  uint8_t MSBmask, uint8_t LSBmask) {
  uint8_t origMSB, origLSB;
  esp_err_t err = max17048_read_reg(reg_addr, &origMSB, &origLSB);
  MSB &= MSBmask;
  LSB &= LSBmask;
  MSB |= origMSB & ~MSBmask;
  LSB |= origLSB & ~LSBmask;
  return err | max17048_write_reg(reg_addr, MSB, LSB);
}
