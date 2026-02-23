#ifndef I2C_HELPER_H
#define I2C_HELPER_H
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// I2C Configuration (Match your schematic)
#define I2C_MASTER_SCL_IO           GPIO_NUM_23   // Example GPIO for C6
#define I2C_MASTER_SDA_IO           GPIO_NUM_22  // Example GPIO for C6
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          100000        // use standard freq 
#define I2C_MASTER_TIMEOUT_MS       1000

esp_err_t i2c_init();
esp_err_t max17048_read_reg(uint8_t reg_addr, uint8_t *MSB, uint8_t *LSB);
esp_err_t max17048_write_reg(uint8_t reg_addr, uint8_t MSB, uint8_t LSB);
esp_err_t max17048_friendly_write_reg(uint8_t reg_addr, uint8_t MSB, uint8_t LSB,
                                      uint8_t MSBmask, uint8_t LSBmask);

#ifdef __cplusplus
}
#endif

#endif