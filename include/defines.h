#ifndef DEFINES_H
#define DEFINES_H
#include "driver/gpio.h"
#include "driver/ledc.h"

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