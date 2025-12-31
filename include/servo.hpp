#ifndef SERVO_H
#define SERVO_H
#include <atomic>
#include "calibration.hpp"
#include "encoder.hpp"

#define CCW 1
#define CW 0
#define server 1
#define manual 0

extern Encoder* topEnc;
extern Encoder* bottomEnc;
extern std::atomic<bool> calibListen;
extern std::atomic<bool> runningManual;
extern std::atomic<bool> runningServer;
extern std::atomic<bool> clearCalibFlag;
extern std::atomic<bool> savePosFlag;
extern std::atomic<bool> startLess;

extern std::atomic<int32_t> baseDiff;
extern std::atomic<int32_t> target;

void servoInit(Encoder& bottom, Encoder& top);
void servoOn(uint8_t dir, uint8_t manOrServer);
void servoOff();
void servoMainSwitch(uint8_t onOff);
void servoSavePos();
void servoCalibListen();
bool servoInitCalib();
bool servoBeginDownwardCalib();
bool servoCompleteCalib();

void initMainLoop();
void watchdogCallback(void* arg);
void servoSavePos();
int32_t servoReadPos();
void stopServerRun();
void servoWandListen();

void runToAppPos(uint8_t appPos);

#endif