#ifndef SOCKETIO_HPP
#define SOCKETIO_HPP
#include <atomic>

extern std::atomic<bool> statusResolved;
extern std::atomic<bool> connected;

// Initialize Socket.IO client and connect to server
void initSocketIO();

// Emit calibration done event to server
void emitCalibDone(int port);

#endif // SOCKETIO_HPP