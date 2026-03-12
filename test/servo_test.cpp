#include "servo_test.hpp"
#include "defines.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void servo_test() {
  // LEDC timer
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode      = LEDC_LOW_SPEED_MODE;
  ledc_timer.timer_num       = LEDC_TIMER_0;
  ledc_timer.duty_resolution = LEDC_TIMER_16_BIT;
  ledc_timer.freq_hz         = 50;
  ledc_timer.clk_cfg         = LEDC_USE_RC_FAST_CLK;
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  // LEDC channel
  ledc_channel_config_t ledc_channel = {};
  ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_channel.channel    = servoLEDCChannel;
  ledc_channel.timer_sel  = LEDC_TIMER_0;
  ledc_channel.intr_type  = LEDC_INTR_DISABLE;
  ledc_channel.gpio_num   = servoPin;
  ledc_channel.duty       = offSpeed;
  ledc_channel.hpoint     = 0;
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

  // Servo switch
  gpio_reset_pin(servoSwitch);
  gpio_set_direction(servoSwitch, GPIO_MODE_OUTPUT);
  gpio_set_level(servoSwitch, 0);

  // Debug LED
  gpio_reset_pin(debugLED);
  gpio_set_direction(debugLED, GPIO_MODE_OUTPUT);
  gpio_set_level(debugLED, 0);

  // Cycle: CW on -> off -> CCW on -> off -> ...
  static const uint32_t duties[4] = { cwSpeed, offSpeed, ccwSpeed, offSpeed };
  static const bool     switches[4] = { true, false, true, false };

  int step = 0;
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    ledc_set_duty(LEDC_LOW_SPEED_MODE, servoLEDCChannel, duties[step]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, servoLEDCChannel);
    gpio_set_level(servoSwitch, switches[step] ? 1 : 0);
    gpio_set_level(debugLED,    switches[step] ? 1 : 0);

    step = (step + 1) % 4;
  }
}
