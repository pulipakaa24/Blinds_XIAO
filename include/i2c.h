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

#ifdef __cplusplus
}
#endif

#endif