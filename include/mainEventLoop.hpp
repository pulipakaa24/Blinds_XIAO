#ifndef BM_EVENTS_H
#define BM_EVENTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// Shared with max17048.c (C) — only C-compatible types here
typedef enum {
  EVENT_CLEAR_CALIB,
  EVENT_SAVE_POS,
  EVENT_REQUEST_POS,
  EVENT_BATTERY_CRITICAL, // stop servo, save pos, alert server, deep sleep
  EVENT_BATTERY_WARNING,  // alert server (no shutdown)
  EVENT_REPORT_SOC,       // periodic SOC sync to server
} main_event_type_t;

extern QueueHandle_t main_event_queue;
extern TaskHandle_t  wakeTaskHandle;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
void mainEventLoop();
void wakeTimer(void* pvParameters);
#endif

#endif
