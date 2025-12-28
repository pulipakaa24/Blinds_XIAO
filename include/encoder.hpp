#ifndef ENCODER_H
#define ENCODER_H
#include <atomic>
#include "esp_pm.h"

extern volatile int32_t encoder_count;
extern esp_pm_lock_handle_t encoder_pm_lock;

void encoder_init();

#endif