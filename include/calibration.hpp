#ifndef CALIBRATION_H
#define CALIBRATION_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include "encoder.hpp"

class Calibration {
  public:
    static void init();
    static bool beginDownwardCalib(Encoder& topEnc);
    static bool completeCalib(Encoder& topEnc);
    static int32_t convertToTicks(uint8_t appPos);
    static uint8_t convertToAppPos(int32_t ticks);
    static bool getCalibrated() {return calibrated;}
    static bool clearCalibrated();
    static std::atomic<int32_t> DownTicks;
    static std::atomic<int32_t> UpTicks;

  private:
    static std::atomic<bool> calibrated;
};

extern TaskHandle_t calibTaskHandle;
bool calibrate();

#endif