#ifndef DEFINES_H
#define DEFINES_H
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Task priorities
#define SERVO_TASK_PRIORITY      (tskIDLE_PRIORITY + 4)  // Highest - real-time control
#define ENCODER_TASK_PRIORITY    (tskIDLE_PRIORITY + 3)  // High - encoder processing
#define SOCKETIO_TASK_PRIORITY   (tskIDLE_PRIORITY + 2)  // Medium
#define BLE_TASK_PRIORITY        (tskIDLE_PRIORITY + 1)  // Low
#define MAIN_TASK_PRIORITY       (tskIDLE_PRIORITY + 1)  // Low

// Event bits for system events
#define EVENT_WIFI_CONNECTED        BIT0
#define EVENT_SOCKETIO_CONNECTED    BIT1
#define EVENT_SOCKETIO_DISCONNECTED BIT2
#define EVENT_CLEAR_CALIB           BIT3
#define EVENT_SAVE_POSITION         BIT4
#define EVENT_BLE_SCAN_REQUEST      BIT5
#define EVENT_BLE_CREDS_RECEIVED    BIT6
#define EVENT_BLE_TOKEN_RECEIVED    BIT7

// Global synchronization primitives
extern EventGroupHandle_t g_system_events;
extern QueueHandle_t g_servo_command_queue;
extern QueueHandle_t g_encoder_event_queue;
extern SemaphoreHandle_t g_calibration_mutex;

#define ccwSpeed 6500
#define cwSpeed 3300
#define offSpeed 4900

#define ccwMax 10
#define cwMax 0

#define nvsWiFi "WiFiCreds"
#define ssidTag "SSID"
#define passTag "PW"
#define authTag "AuthMode"
#define unameTag "UNAME"

#define nvsAuth "AUTH"
#define tokenTag "TOKEN"

#define nvsCalib "CALIB"
#define UpTicksTag "UP"
#define DownTicksTag "DOWN"
#define statusTag "STATUS"

#define nvsServo "SERVO"
#define posTag "POS"

#define ENCODER_PIN_A GPIO_NUM_23
#define ENCODER_PIN_B GPIO_NUM_16

#define InputEnc_PIN_A GPIO_NUM_1
#define InputEnc_PIN_B GPIO_NUM_2

#define servoPin GPIO_NUM_20
#define servoLEDCChannel LEDC_CHANNEL_0
#define servoSwitch GPIO_NUM_17

#endif