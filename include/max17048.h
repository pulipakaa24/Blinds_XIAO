#ifndef MAX_17_H
#define MAX_17_H

#define MAX17048_ADDR        0x36
#define MAX17048_REG_VCELL   0x02
#define MAX17048_REG_SOC     0x04
#define MAX17048_REG_MODE    0x06
#define MAX17048_REG_VERSION 0x08
#define MAX17048_REG_CONFIG  0x0C
#define MAX17048_REG_VALRT   0x14
#define MAX17048_REG_VRST_ID 0x18
#define MAX17048_REG_STATUS  0x1A
#define MAX17048_REG_CMD     0xFE

#define MAX17048_CMD_POR     0x5400
#define MAX17048_CMD_QSTRT   0x4000

// NOTE: maxALRT (GPIO_NUM_2) conflicts with InputEnc_PIN_B in defines.h.
// Assign maxALRT to a free GPIO before enabling the interrupt.
#define maxALRT GPIO_NUM_2

// STATUS register MSB (addr 0x1A) bit masks
// [7:X] [6:EnVR] [5:SC] [4:HD] [3:VR] [2:VL] [1:VH] [0:RI]
#define SCbit  (1 << 5)  // SOC changed by 1%
#define HDbit  (1 << 4)  // SOC crossed low threshold (CONFIG.ATHD)
#define VRbit  (1 << 3)  // voltage reset alert
#define VLbit  (1 << 2)  // VCELL below VALRT.MIN
#define VHbit  (1 << 1)  // VCELL above VALRT.MAX
#define RIbit  (1 << 0)  // reset indicator (device just powered on)

// SOC thresholds for user-facing push notifications / shutdown logic
#define SOC_WARN_20     20   // emit warning push at this level
#define SOC_WARN_10     10   // emit critical push at this level
#define SOC_CRITICAL_VL  5   // treat undervoltage as critical only below this SOC

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"

// Alert type carried from bms_checker_task to mainEventLoop via bms_pending_alert
typedef enum {
  BATT_ALERT_OVERVOLTAGE,         // VH: charging fault / damaged battery
  BATT_ALERT_CRITICAL_LOW,        // HD, or VL + SOC < SOC_CRITICAL_VL
  BATT_ALERT_LOW_VOLTAGE_WARNING, // VL with SOC >= SOC_CRITICAL_VL (transient dip)
  BATT_ALERT_SOC_LOW_20,          // SOC just crossed 20% downward
  BATT_ALERT_SOC_LOW_10,          // SOC just crossed 10% downward
} batt_alert_type_t;

extern uint8_t established_soc;
extern volatile batt_alert_type_t bms_pending_alert;

esp_err_t max17048_init();
uint8_t   bms_get_soc();
esp_err_t bms_set_alert_bound_voltages(float min, float max);
esp_err_t bms_set_reset_voltage(float vreset);
void      bms_checker_task(void *pvParameters);

#define bms_set_alsc()     max17048_friendly_write_reg(MAX17048_REG_CONFIG, 0, 1<<6, 0, 1<<6)
#define bms_clear_status() max17048_write_reg(MAX17048_REG_STATUS, 0, 0)
#define bms_clear_alrt()   max17048_friendly_write_reg(MAX17048_REG_CONFIG, 0, 0, 0, 1<<5)

esp_err_t max17048_read_reg(uint8_t reg_addr, uint8_t *MSB, uint8_t *LSB);
esp_err_t max17048_write_reg(uint8_t reg_addr, uint8_t MSB, uint8_t LSB);
esp_err_t max17048_friendly_write_reg(uint8_t reg_addr, uint8_t MSB, uint8_t LSB,
                                      uint8_t MSBmask, uint8_t LSBmask);

#ifdef __cplusplus
}
#endif

#endif
