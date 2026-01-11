#ifndef SETUP_H
#define SETUP_H

extern TaskHandle_t setupTaskHandle;
extern SemaphoreHandle_t Setup_Complete_Semaphore;

void initialSetup();
void setupLoop(void *pvParameters);

#endif