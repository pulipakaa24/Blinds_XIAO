// #include "lp_i2c.h"
// #include "ulp_lp_core.h"
// #include "lp_core_i2c.h"
// #include "lp_core_src.h"

// extern const uint8_t lp_core_main_bin_start[] asm("_binary_lp_core_main_bin_start");
// extern const uint8_t lp_core_main_bin_end[]   asm("_binary_lp_core_main_bin_end");

// void lp_i2c_init()
// {
//   esp_err_t ret = ESP_OK;

//   /* Initialize LP I2C with default configuration */
//   const lp_core_i2c_cfg_t i2c_cfg = LP_CORE_I2C_DEFAULT_CONFIG();
//   ret = lp_core_i2c_master_init(LP_I2C_NUM_0, &i2c_cfg);
//   if (ret != ESP_OK) {
//     printf("LP I2C init failed\n");
//     abort();
//   }

//   printf("LP I2C initialized successfully\n");
// }

// void lp_core_init()
// {
//   esp_err_t ret = ESP_OK;

//   ulp_lp_core_cfg_t cfg = {
//     .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_HP_CPU,
//   };

//   ret = ulp_lp_core_load_binary(lp_core_main_bin_start, (lp_core_main_bin_end - lp_core_main_bin_start));
//   if (ret != ESP_OK) {
//     printf("LP Core load failed\n");
//     abort();
//   }

//   ret = ulp_lp_core_run(&cfg);
//   if (ret != ESP_OK) {
//     printf("LP Core run failed\n");
//     abort();
//   }

//   printf("LP core loaded with firmware successfully\n");
// }