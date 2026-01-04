#include "encoder.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/gpio_struct.h"
#include "servo.hpp"
#include "defines.h"

static const char *TAG = "ENCODER";

// Constructor
Encoder::Encoder(gpio_num_t pinA, gpio_num_t pinB) 
    : pin_a(pinA), pin_b(pinB), count(0), 
      last_state_a(0), last_state_b(0), last_count_base(0) {}

// Static ISR - receives Encoder instance via arg
void IRAM_ATTR Encoder::isr_handler(void* arg)
{
  Encoder* encoder = static_cast<Encoder*>(arg);
  
  // Read GPIO levels directly from hardware
  uint32_t gpio_levels = GPIO.in.val;
  uint8_t current_a = (gpio_levels >> encoder->pin_a) & 0x1;
  uint8_t current_b = (gpio_levels >> encoder->pin_b) & 0x1;

  // Quadrature decoding logic
  if (current_a != encoder->last_state_a) {
    if (!current_a) { 
      if (current_b) encoder->last_count_base++;
      else encoder->last_count_base--;
    }
    else { 
      if (current_b) encoder->last_count_base--;
      else encoder->last_count_base++;
    }
  }
  else if (current_b != encoder->last_state_b) {
    if (!current_b) { 
      if (current_a) encoder->last_count_base--;
      else encoder->last_count_base++;
    }
    else { 
      if (current_a) encoder->last_count_base++;
      else encoder->last_count_base--;
    }
  }
  
  // Accumulate to full detent count
  if (encoder->last_count_base > 3) {
    encoder->count += 1;
    encoder->last_count_base -= 4;
    
    // DEFER to task via queue instead of direct function calls
    encoder_event_t event = {
        .count = encoder->count.load(),
        .is_top_encoder = (encoder == topEnc)
    };
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (g_encoder_event_queue != NULL) {
      xQueueSendFromISR(g_encoder_event_queue, &event, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
  else if (encoder->last_count_base < 0) {
    encoder->count -= 1;
    encoder->last_count_base += 4;
    
    // DEFER to task via queue instead of direct function calls
    encoder_event_t event = {
        .count = encoder->count.load(),
        .is_top_encoder = (encoder == topEnc)
    };
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (g_encoder_event_queue != NULL) {
      xQueueSendFromISR(g_encoder_event_queue, &event, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
  
  encoder->last_state_a = current_a;
  encoder->last_state_b = current_b;
}

void Encoder::init()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << pin_a) | (1ULL << pin_b);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; 
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Install ISR service if not already installed
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);

    // Attach ISR with THIS instance as argument
    gpio_isr_handler_add(pin_a, Encoder::isr_handler, this);
    gpio_isr_handler_add(pin_b, Encoder::isr_handler, this);

    ESP_LOGI(TAG, "Encoder initialized on pins %d and %d", pin_a, pin_b);
}

void Encoder::deinit()
{
    gpio_isr_handler_remove(pin_a);
    gpio_isr_handler_remove(pin_b);
    ESP_LOGI(TAG, "Encoder deinitialized");
}

void Encoder::setupWatchdog() {
  if (watchdog_handle == NULL) {
    const esp_timer_create_args_t enc_watchdog_args = {
      .callback = &watchdogCallback,
      .dispatch_method = ESP_TIMER_ISR,
      .name = "encoder_wdt",
    };

    ESP_ERROR_CHECK(esp_timer_create(&enc_watchdog_args, &watchdog_handle));
  }

  ESP_ERROR_CHECK(esp_timer_start_once(watchdog_handle, 500000));
  feedWDog = true;
}

void Encoder::pauseWatchdog() {
  feedWDog = false;
  if (watchdog_handle != NULL) esp_timer_stop(watchdog_handle);
}

Encoder::~Encoder() {
  if (watchdog_handle != NULL) {
    esp_timer_stop(watchdog_handle);
    esp_timer_delete(watchdog_handle);
    watchdog_handle = NULL;
  }
}