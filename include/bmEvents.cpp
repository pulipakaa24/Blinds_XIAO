#include "bmEvents.hpp"
#include <portmacro.h>
#include <freertos/projdefs.h>

// blueprint for sending events... maybe I never use this. We'll see.
// bool send_app_event(app_event_t *event, QueueHandle_t event_queue) {
//   if (event_queue == NULL) return false;

//   // A. Are we in an Interrupt Service Routine (Hardware context)?
//   if (xPortInIsrContext()) {
//     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
//     // Use the "FromISR" version
//     BaseType_t result = xQueueSendFromISR(event_queue, event, &xHigherPriorityTaskWoken);
    
//     // This is crucial for "Instant Wakeup"
//     if (result == pdPASS) portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//     return (result == pdPASS);
//   } 
  
//   // B. Are we in a Standard Task (WiFi/BLE Callback context)?
//   else {
//     // Use the standard version
//     // 10 ticks wait time is arbitrary; allows a small buffer if queue is full
//     return (xQueueSend(event_queue, event, pdMS_TO_TICKS(10)) == pdPASS);
//   }
// }


// blueprint for deleting more complex event queues
// void deinit_BLE_event_queue(QueueHandle_t& event_queue) {
//   if (event_queue != NULL) {
//     app_event_t temp_event;
//     while (xQueueReceive(event_queue, &temp_event, 0) == pdTRUE) {
//       if (temp_event.data != NULL) free(temp_event.data);
//     }

//     vQueueDelete(event_queue);

//     event_queue = NULL;
    
//     printf("Event queue deleted.\n");
//   }
// }