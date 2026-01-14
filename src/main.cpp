#include <driver/gptimer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "servo.hpp"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "WiFi.hpp"
#include "setup.hpp"
#include "socketIO.hpp"
#include "encoder.hpp"
#include "calibration.hpp"
#include "esp_pm.h"
#include "mainEventLoop.hpp"

// Global encoder instances
Encoder* topEnc = new Encoder(ENCODER_PIN_A, ENCODER_PIN_B);
Encoder* bottomEnc = new Encoder(InputEnc_PIN_A, InputEnc_PIN_B);

// RTC_NOINIT_ATTR volatile uint8_t shared_battery_soc;

void mainApp() {
  esp_err_t ret = nvs_flash_init(); // change to secure init logic soon!!
  // 2. If NVS is full or corrupt (common after flashing new code), erase and retry
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  WiFi::init();
  Calibration::init();
  
  // Initialize encoders
  topEnc->init();
  bottomEnc->init();
  servoInit();

  // printf("Loading LP Core Firmware for battery percentage\n");

  setupAndCalibrate();

  xTaskCreate(wakeTimer, "wakeTimer", 2048, NULL, 5, &wakeTaskHandle);

  mainEventLoop();
}

void pm_init() {
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,        // Max CPU frequency
        .min_freq_mhz = 80,         // Min CPU frequency (DFS)
        .light_sleep_enable = true  // ALLOW CPU to power down during idle
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

#define RPin GPIO_NUM_0
#define BPin GPIO_NUM_21
#define GPin GPIO_NUM_18

void flashLED() {
  gpio_set_direction(RPin, GPIO_MODE_OUTPUT);
  gpio_set_direction(GPin, GPIO_MODE_OUTPUT);
  gpio_set_direction(BPin, GPIO_MODE_OUTPUT);
  gpio_sleep_sel_dis(RPin);
  gpio_sleep_sel_dis(GPin);
  gpio_sleep_sel_dis(BPin);
  
  // Start with all LEDs off
  gpio_set_level(RPin, 0);
  gpio_set_level(BPin, 0);
  gpio_set_level(GPin, 0);

  while (1) {
    printf("Step1\n");
    // Red ON
    gpio_set_level(RPin, 1);
    gpio_set_level(GPin, 0);
    gpio_set_level(BPin, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Step2\n");
    // Green ON
    gpio_set_level(RPin, 0);
    gpio_set_level(GPin, 1);
    gpio_set_level(BPin, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Step3\n");
    // Blue ON
    gpio_set_level(RPin, 0);
    gpio_set_level(GPin, 0);
    gpio_set_level(BPin, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("Step4\n");
    // All OFF
    gpio_set_level(RPin, 0);
    gpio_set_level(GPin, 0);
    gpio_set_level(BPin, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

extern "C" void app_main() {
  // pm_init();
  // mainApp();
  flashLED();
}