#include <driver/gptimer.h>
#include "servo.hpp"
#include "defines.h"
#include "nvs_flash.h"
#include "NimBLEDevice.h"
#include "WiFi.hpp"
#include "setup.hpp"
#include "socketIO.hpp"
#include "encoder.hpp"
#include "calibration.hpp"

// Global synchronization primitives
EventGroupHandle_t g_system_events = NULL;
QueueHandle_t g_servo_command_queue = NULL;
QueueHandle_t g_encoder_event_queue = NULL;
SemaphoreHandle_t g_calibration_mutex = NULL;

// Global encoder instances
Encoder* topEnc = new Encoder(ENCODER_PIN_A, ENCODER_PIN_B);
Encoder* bottomEnc = new Encoder(InputEnc_PIN_A, InputEnc_PIN_B);

// Global calibration instance
Calibration calib;

void mainTask(void* arg) {
  EventBits_t bits;
  
  printf("Main task started - event-driven mode\n");
  
  while (1) {
    // Block waiting for ANY event (no polling!)
    bits = xEventGroupWaitBits(
        g_system_events,
        EVENT_SOCKETIO_DISCONNECTED | EVENT_CLEAR_CALIB | EVENT_SAVE_POSITION,
        pdTRUE,  // Clear bits on exit
        pdFALSE, // Wait for ANY bit (not all)
        portMAX_DELAY  // Block forever - no polling!
    );
    
    if (bits & EVENT_SOCKETIO_DISCONNECTED) {
      if (!connected) {
        printf("Disconnected! Beginning setup loop.\n");
        stopSocketIO();
        setupLoop();
      }
      else {
        printf("Reconnected!\n");
      }
    }
    
    if (bits & EVENT_CLEAR_CALIB) {
      xSemaphoreTake(g_calibration_mutex, portMAX_DELAY);
      calib.clearCalibrated();
      xSemaphoreGive(g_calibration_mutex);
      emitCalibStatus(false);
    }
    
    if (bits & EVENT_SAVE_POSITION) {
      servoSavePos();

      // Send position update to server
      uint8_t currentAppPos = calib.convertToAppPos(topEnc->getCount());
      emitPosHit(currentAppPos);
      
      printf("Sent pos_hit: position %d\n", currentAppPos);
    }
  }
}

void encoderTest() {
  // Create encoder instance
  Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);
  encoder.init();

  int32_t prevCount = encoder.getCount();

  while (1) {
    int32_t currentCount = encoder.getCount();
    if (currentCount != prevCount) {
      prevCount = currentCount;
      printf("Encoder Pos: %d\n", prevCount);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

extern "C" void app_main() {
  // Initialize NVS first
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Create synchronization primitives
  g_system_events = xEventGroupCreate();
  g_servo_command_queue = xQueueCreate(10, sizeof(servo_cmd_msg_t));
  g_encoder_event_queue = xQueueCreate(50, sizeof(encoder_event_t));
  g_calibration_mutex = xSemaphoreCreateMutex();
  
  if (g_system_events == NULL || g_servo_command_queue == NULL || 
      g_encoder_event_queue == NULL || g_calibration_mutex == NULL) {
    printf("ERROR: Failed to create synchronization primitives\n");
    return;
  }

  // Initialize hardware
  bmWiFi.init();
  calib.init();
  
  // Initialize encoders
  topEnc->init();
  bottomEnc->init();
  servoInit();

  // Create servo control task (highest priority - real-time)
  xTaskCreate(servoControlTask, "servo_ctrl", 4096, NULL, SERVO_TASK_PRIORITY, NULL);
  
  // Create main task (lower priority)
  xTaskCreate(mainTask, "main", 4096, NULL, MAIN_TASK_PRIORITY, NULL);
  
  // Run setup loop (this will handle WiFi/SocketIO connection)
  setupLoop();
  
  // app_main returns, but tasks continue running
  printf("app_main complete - tasks running\n");
}