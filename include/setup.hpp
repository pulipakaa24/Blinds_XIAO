#ifndef SETUP_H
#define SETUP_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>

extern TaskHandle_t setupTaskHandle;
extern std::atomic<bool> awaitCalibration;

void initialSetup();
void setupLoop();

void setupAndCalibrate();

#endif