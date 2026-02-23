#ifndef MAX_17_H
#define MAX_17_H

#define MAX17048_ADDR           0x36
#define MAX17048_REG_VCELL      0x02 // Voltage
#define MAX17048_REG_SOC        0x04 // State of Charge (%)
#define MAX17048_REG_MODE       0x06
#define MAX17048_REG_VERSION    0x08
#define MAX17048_REG_CONFIG     0x0C
#define MAX17048_REG_CMD        0xFE // Command (Reset)
#define MAX17048_REG_STATUS     0x1A
#define MAX17048_REG_VALRT      0x14
#define MAX17048_REG_VRST_ID    0x18

// Commands
#define MAX17048_CMD_POR        0x5400 // Power On Reset command
#define MAX17048_CMD_QSTRT      0x4000 // Quick Start command


#ifdef __cplusplus
extern "C" {
#endif
#include "driver/i2c.h"

esp_err_t max17048_init();
uint8_t bms_get_soc();
esp_err_t bms_set_alert_bound_voltages(float min, float max);
esp_err_t bms_set_reset_voltage(float vreset);
esp_err_t bms_clear_status();

#define bms_set_alsc() max17048_friendly_write_reg(MAX17048_REG_CONFIG, 0, 1<<6, 0, 1<<6)
#define bms_clear_status() max17048_write_reg(MAX17048_REG_STATUS, 0, 0)

#ifdef __cplusplus
}
#endif

#endif