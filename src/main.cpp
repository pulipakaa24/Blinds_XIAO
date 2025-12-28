#include <driver/gptimer.h>
#include "pwm.h"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "WiFi.hpp"
#include "setup.hpp"
#include "socketIO.hpp"
#include "encoder.hpp"
#include "esp_pm.h"
#include "esp_log.h"
#include <esp_sleep.h>

void mainApp() {
  printf("Hello ");
  esp_err_t ret = nvs_flash_init(); // change to secure init logic soon!!
  // 2. If NVS is full or corrupt (common after flashing new code), erase and retry
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  bmWiFi.init();

  setupLoop();
  
  statusResolved = false;

  // Main loop
  while (1) {
    if (statusResolved) {
      if (!connected) {
        printf("Disconnected! Beginning setup loop.\n");
        stopSocketIO();
        setupLoop();
      }
      else printf("Reconnected!\n");
      statusResolved = false;
    }
    // Your main application logic here
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("loop\n");
  }
}

// XIAO ESP32C6 User LED
#define LED_PIN GPIO_NUM_15 

esp_pm_lock_handle_t keep_alive_lock = NULL;

void enable_power_management(void)
{
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,        // Scales down when idle
        .light_sleep_enable = true // Enters sleep automatically
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

void encoderTest() {
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    encoder_init();
    enable_power_management();

    // Create a "Keep Alive" lock for the USB
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "usb_keep_alive", &keep_alive_lock);
    
    // Acquire lock immediately so we can see the boot message
    esp_pm_lock_acquire(keep_alive_lock);
    bool is_awake = true;
    
    int32_t prevCount = encoder_count;
    int64_t last_activity = esp_timer_get_time();

    printf("\n=== ENCODER SLEEP DEMO ===\n");
    printf("1. Turn knob: Counts update, USB stays alive.\n");
    printf("2. Stop turning: After 5s, device sleeps (USB disconnects).\n");
    printf("3. Turn again: LED flashes instantly (Chip woke up!).\n");

    while (1) {
        // DETECT MOTION
        if (encoder_count != prevCount) {
            // Visual check: LED Toggles on every step
            gpio_set_level(LED_PIN, encoder_count % 2); 

            // If we were "asleep" (logically), wake up fully
            if (!is_awake) {
                esp_pm_lock_acquire(keep_alive_lock);
                is_awake = true;
                printf("\n[WAKE UP] USB reconnecting (give it a second)...\n");
            }

            last_activity = esp_timer_get_time();
            printf("Pos: %ld\n", (long)encoder_count);
            prevCount = encoder_count;
        }

        // DETECT IDLE (5 Seconds)
        if (is_awake && (esp_timer_get_time() - last_activity) > 5000000) {
            printf("[SLEEPING] Releasing lock in 1s. USB will die.\n");
            vTaskDelay(pdMS_TO_TICKS(1000)); // Flush logs
            
            // Release lock -> Light Sleep -> USB Dies
            esp_pm_lock_release(keep_alive_lock);
            is_awake = false;
            
            // Turn LED off to indicate sleep
            gpio_set_level(LED_PIN, 0); 
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main() {
  encoderTest();
}