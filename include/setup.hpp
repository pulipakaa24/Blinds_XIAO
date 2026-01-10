#ifndef SETUP_H
#define SETUP_H

extern TaskHandle_t setupTaskHandle;

void initialSetup();
void setupLoop(void *pvParameters);

#endif