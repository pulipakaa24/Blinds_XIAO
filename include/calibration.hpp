#ifndef CALIBRATION_H
#define CALIBRATION_H
#include <atomic>
#include "encoder.hpp"

class Calibration {
  public:
    void init();
    bool beginDownwardCalib(Encoder& topEnc);
    bool completeCalib(Encoder& topEnc);
    int32_t convertToTicks(uint8_t appPos);
    uint8_t convertToAppPos(int32_t ticks);
    bool getCalibrated() {return calibrated;}
    bool clearCalibrated();
    std::atomic<int32_t> DownTicks;
    std::atomic<int32_t> UpTicks;

  private:
    std::atomic<bool> calibrated;
};

extern Calibration calib;

#endif