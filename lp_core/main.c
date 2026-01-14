/* lp_core/main.c */
#include <stdio.h>
#include <stdint.h>
#include "lp_core_i2c.h"
// #include "ulp_lp_core_utils.h"
#include "max17048_defs.h"

// Define the timeout for I2C transactions
#define LP_I2C_TIMEOUT_CYCLES 5000

// --- SHARED MEMORY (Visible to both Main CPU and LP Core) ---
// volatile ensures the compiler doesn't optimize these away
volatile uint32_t batt_soc_int = 0;   // Integer % (0-100)
volatile uint32_t batt_volts_mv = 0;  // Voltage in mV
volatile uint32_t update_counter = 0; // Debug counter

// Function to swap bytes (MAX17048 sends MSB first, C6 expects LSB)
static uint16_t swap_bytes(uint8_t *data) {
    return (data[0] << 8) | data[1];
}

int main(void)
{
    esp_err_t ret;
    uint8_t raw_data[2];

    // 1. Read SOC (Register 0x04)
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, MAX17048_I2C_ADDR, 
                                              REG_SOC, // Register Address to read from
                                              raw_data, sizeof(raw_data), 
                                              LP_I2C_TIMEOUT_CYCLES);
    
    if (ret == ESP_OK) {
        uint16_t soc_raw = swap_bytes(raw_data);
        // MAX17048 SOC is 1/256% per bit. We just want the integer part.
        batt_soc_int = soc_raw / 256;
    }

    // 2. Read Voltage (Register 0x02)
    // Note: You must read from the device again for the next register
    ret = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, MAX17048_I2C_ADDR, 
                                              REG_VCELL, 
                                              raw_data, sizeof(raw_data), 
                                              LP_I2C_TIMEOUT_CYCLES);

    if (ret == ESP_OK) {
        uint16_t vcell_raw = swap_bytes(raw_data);
        // MAX17048 Voltage is 78.125uV per bit. 
        // 78.125uV * val ~= val * 0.078125 mV
        batt_volts_mv = (uint32_t)(vcell_raw * 0.078125f);
    }

    update_counter++;

    // 3. Check Thresholds to Wake Main CPU
    // If battery is critically low (< 10%), wake the big CPU to save data/shutdown
    if (batt_soc_int < 10 && batt_soc_int > 0) {
        ulp_lp_core_wakeup_main_processor();
    }

    // 4. Return 0 to halt. 
    // The Main CPU has set a timer to wake us up again in X seconds.
    return 0;
}