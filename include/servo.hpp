#ifndef SERVO_H
#define SERVO_H
#include <atomic>
#include "calibration.hpp"
#include "encoder.hpp"

#define CCW 1
#define CW 0
#define server 1
#define manual 0

extern std::atomic<bool> calibListen;

extern Encoder* topEnc;
extern Encoder* bottomEnc;

void servoInit();
void servoOn(uint8_t dir, uint8_t manOrServer);
void servoOff();
void servoMainSwitch(uint8_t onOff);
void servoSavePos();
void servoCalibListen();
bool servoInitCalib();
bool servoBeginDownwardCalib();
bool servoCompleteCalib();
void servoCancelCalib();
void debugLEDSwitch(uint8_t onOff);
void debugLEDTgl();

void initMainLoop();
void watchdogCallback(void* arg);
void servoSavePos();
int32_t servoReadPos();
void stopServerRun();
void servoWandListen();
void servoServerListen();
void runToAppPos(uint8_t appPos);

#endif