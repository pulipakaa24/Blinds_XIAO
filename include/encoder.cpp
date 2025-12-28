#include "encoder.hpp"
#include "defines.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "soc/gpio_struct.h" // <--- REQUIRED for GPIO.in.val

volatile int32_t encoder_count = 0;
static const char *TAG = "ENCODER_DEMO";

// --- THE SAFE ISR (RAM ONLY) ---
static void IRAM_ATTR encoder_isr_handler(void* arg)
{
    static uint8_t last_state_a = 0;
    static uint8_t last_state_b = 0;

    // READ HARDWARE DIRECTLY: This effectively bypasses the Flash crash risk
    uint32_t gpio_levels = GPIO.in.val;
    uint8_t current_a = (gpio_levels >> ENCODER_PIN_A) & 0x1;
    uint8_t current_b = (gpio_levels >> ENCODER_PIN_B) & 0x1;

    // LOGIC
    if (current_a != last_state_a) {
        if (current_a == 0) { 
            if (current_b == 1) encoder_count++;
            else encoder_count--;
        }
        else { 
            if (current_b == 1) encoder_count--;
            else encoder_count++;
        }
    }
    else if (current_b != last_state_b) {
        if (current_b == 0) { 
            if (current_a == 1) encoder_count--;
            else encoder_count++;
        }
        else { 
            if (current_a == 1) encoder_count++;
            else encoder_count--;
        }
    }
    last_state_a = current_a;
    last_state_b = current_b;
}

void encoder_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << ENCODER_PIN_A) | (1ULL << ENCODER_PIN_B);
    io_conf.mode = GPIO_MODE_INPUT;
    // Internal Pull-ups prevent "floating" inputs waking the chip randomly
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM); // IRAM is vital for Sleep

    gpio_isr_handler_add(ENCODER_PIN_A, encoder_isr_handler, NULL);
    gpio_isr_handler_add(ENCODER_PIN_B, encoder_isr_handler, NULL);

    // Allow these pins to wake the chip from Light Sleep
    gpio_wakeup_enable(ENCODER_PIN_A, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(ENCODER_PIN_B, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    ESP_LOGI(TAG, "Encoder initialized with Sleep Wakeup support.");
}