#ifndef BLE_H
#define BLE_H

#include "NimBLEDevice.h"
#include "cJSON.h"
#include <atomic>
#include <mutex>
#include <string>
#include "esp_wifi_types.h"

// Global pointers to characteristics for notification support
extern NimBLECharacteristic* ssidListChar;
extern NimBLECharacteristic* connectConfirmChar;

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    printf("Client connected\n");
  };
  
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    printf("Client disconnected - reason: %d\n", reason);
    // Advertising will restart automatically
  }
};

class MyCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo);
};

NimBLEAdvertising* initBLE();
bool BLEtick(NimBLEAdvertising* pAdvertising);

#endif