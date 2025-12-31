#ifndef SOCKETIO_HPP
#define SOCKETIO_HPP
#include <atomic>

extern std::atomic<bool> statusResolved;
extern std::atomic<bool> connected;

// Initialize Socket.IO client and connect to server
void initSocketIO();

// Stop and destroy Socket.IO client
void stopSocketIO();

// Emit calibration stage events to server
void emitCalibStatus(bool calibrated, int port = 1);
void emitCalibStage1Ready(int port = 1);
void emitCalibStage2Ready(int port = 1);
void emitCalibDone(int port = 1);
void emitCalibError(const char* errorMessage, int port = 1);
void emitPosHit(int pos, int port = 1);

#endif // SOCKETIO_HPP