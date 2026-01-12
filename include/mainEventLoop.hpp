#ifndef BM_EVENTS_H
#define BM_EVENTS_H
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Event Types
typedef enum {
    EVENT_CLEAR_CALIB,
    EVENT_SAVE_POS,
    EVENT_REQUEST_POS
} main_event_type_t;

void mainEventLoop();

extern QueueHandle_t main_event_queue;

void wakeTimer(void* pvParameters);

extern TaskHandle_t wakeTaskHandle;

#endif