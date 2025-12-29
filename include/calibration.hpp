#ifndef CALIBRATION_H
#define CALIBRATION_H
#include <atomic>
#include "encoder.hpp"

class Calibration {
  public:
    void init();
    void beginDownwardCalib() {startTicks = topEnc.getCount();}
    bool completeCalib();
    int32_t convertToTicks(int8_t steps10);
    bool getCalibrated() {return calibrated;}
    void clearCalibrated();
    Calibration(Encoder& enc):topEnc(enc) {};

  private:
    std::atomic<bool> calibrated;
    std::atomic<int32_t> UpMinusDownTicks;
    int32_t startTicks;
    Encoder& topEnc;
};

extern Calibration calib;

#endif