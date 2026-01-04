#ifndef SERVO_H
#define SERVO_H
#include <atomic>
#include "calibration.hpp"
#include "encoder.hpp"

#define CCW 1
#define CW 0
#define server 1
#define manual 0

// Command structures for servo control
typedef enum {
    SERVO_CMD_STOP,
    SERVO_CMD_MOVE_CCW,
    SERVO_CMD_MOVE_CW,
    SERVO_CMD_MOVE_TO_POSITION
} servo_command_t;

typedef struct {
    servo_command_t command;
    uint8_t target_position;  // For MOVE_TO_POSITION
    bool is_manual;           // vs server-initiated
} servo_cmd_msg_t;

extern std::atomic<bool> calibListen;
extern std::atomic<bool> clearCalibFlag;
extern std::atomic<bool> savePosFlag;

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

void initMainLoop();
void watchdogCallback(void* arg);
void servoSavePos();
int32_t servoReadPos();
void stopServerRun();
void servoWandListen();
void servoServerListen();
void runToAppPos(uint8_t appPos);

// Servo control task
void servoControlTask(void* arg);

#endif