#include "encoder_test.hpp"
#include "defines.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/gpio_struct.h"

typedef struct {
  uint8_t encoder_id; // 0 = top, 1 = bottom
  int8_t  direction;  // +1 or -1
} enc_event_t;

typedef struct {
  gpio_num_t pin_a;
  gpio_num_t pin_b;
  uint8_t    id;
  uint8_t    last_a;
  uint8_t    last_b;
  int8_t     base;
} enc_state_t;

static QueueHandle_t enc_queue;
static enc_state_t   enc_states[2];
static bool          led_state = false;

static void IRAM_ATTR enc_isr(void* arg) {
  enc_state_t* s = (enc_state_t*)arg;

  uint32_t levels = GPIO.in.val;
  uint8_t a = (levels >> s->pin_a) & 1;
  uint8_t b = (levels >> s->pin_b) & 1;

  if (a != s->last_a) {
    if (!a) s->base += b ? 1 : -1;
    else    s->base += b ? -1 : 1;
  } else if (b != s->last_b) {
    if (!b) s->base += a ? -1 : 1;
    else    s->base += a ? 1 : -1;
  }

  s->last_a = a;
  s->last_b = b;

  enc_event_t evt;
  evt.encoder_id = s->id;
  BaseType_t woken = pdFALSE;

  if (s->base >= 4) {
    s->base -= 4;
    evt.direction = 1;
    led_state = !led_state;
    gpio_set_level(debugLED, led_state);
    xQueueSendFromISR(enc_queue, &evt, &woken);
    portYIELD_FROM_ISR(woken);
  } else if (s->base <= -4) {
    s->base += 4;
    evt.direction = -1;
    led_state = !led_state;
    gpio_set_level(debugLED, led_state);
    xQueueSendFromISR(enc_queue, &evt, &woken);
    portYIELD_FROM_ISR(woken);
  }
}

void encoder_test() {
  // LED
  gpio_reset_pin(debugLED);
  gpio_set_direction(debugLED, GPIO_MODE_OUTPUT);
  gpio_set_level(debugLED, 0);

  enc_queue = xQueueCreate(32, sizeof(enc_event_t));

  // Init encoder states
  enc_states[0] = { ENCODER_PIN_A, ENCODER_PIN_B, 0, 0, 0, 0 };
  enc_states[1] = { InputEnc_PIN_A, InputEnc_PIN_B, 1, 0, 0, 0 };

  // Configure encoder pins
  gpio_config_t io_conf = {};
  io_conf.intr_type    = GPIO_INTR_ANYEDGE;
  io_conf.mode         = GPIO_MODE_INPUT;
  io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pin_bit_mask = (1ULL << ENCODER_PIN_A) | (1ULL << ENCODER_PIN_B)
                       | (1ULL << InputEnc_PIN_A) | (1ULL << InputEnc_PIN_B);
  gpio_config(&io_conf);

  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
  gpio_isr_handler_add(ENCODER_PIN_A,  enc_isr, &enc_states[0]);
  gpio_isr_handler_add(ENCODER_PIN_B,  enc_isr, &enc_states[0]);
  gpio_isr_handler_add(InputEnc_PIN_A, enc_isr, &enc_states[1]);
  gpio_isr_handler_add(InputEnc_PIN_B, enc_isr, &enc_states[1]);

  printf("Encoder test running...\n");

  enc_event_t evt;
  while (true) {
    if (xQueueReceive(enc_queue, &evt, portMAX_DELAY) == pdTRUE) {
      printf("[%s] %s\n",
        evt.encoder_id == 0 ? "TOP   " : "BOTTOM",
        evt.direction  == 1 ? "CW  (+1)" : "CCW (-1)");
    }
  }
}
