/* max17048_defs.h */
#pragma once

#define MAX17048_I2C_ADDR       0x36

// Registers
#define REG_VCELL               0x02
#define REG_SOC                 0x04
#define REG_MODE                0x06
#define REG_VERSION             0x08
#define REG_CONFIG              0x0C
#define REG_STATUS              0x1A
#define REG_CMD                 0xFE

// Commands / Masks
#define CMD_CLEAR_ALERT         0x0080 // Mask to clear ALRT bit in Status Reg
#define CMD_QUICK_START         0x4000