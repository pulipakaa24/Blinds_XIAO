#ifndef ENCODER_H
#define ENCODER_H
#include "driver/gpio.h"
#include <atomic>
#include "esp_timer.h"

// Encoder event structure for queue
typedef struct {
    int32_t count;
    bool is_top_encoder;
} encoder_event_t;

class Encoder {
public:
  // Shared between ISR and main code
  std::atomic<int32_t> count;
  
  // ISR-only state
  uint8_t last_state_a;
  uint8_t last_state_b;
  int8_t last_count_base;
  
  // Configuration
  gpio_num_t pin_a;
  gpio_num_t pin_b;
  
  // Static ISR that receives instance pointer via arg
  static void isr_handler(void* arg);

  std::atomic<bool> feedWDog;
  std::atomic<bool> serverListen;
  std::atomic<bool> wandListen;

  esp_timer_handle_t watchdog_handle;
  
  // Constructor and methods
  Encoder(gpio_num_t pinA, gpio_num_t pinB);
  void init();
  int32_t getCount() const { return count; }
  void setCount(int32_t value) { count = value; }
  void deinit();

  void setupWatchdog();
  void pauseWatchdog();

  ~Encoder();
};

#endif