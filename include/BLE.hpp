#ifndef BLE_H
#define BLE_H

#include "NimBLEDevice.h"
#include "cJSON.h"
#include <atomic>
#include <string>
#include "esp_wifi_types.h"

// Global pointers to characteristics for notification support
extern std::atomic<NimBLECharacteristic*> ssidListChar;
extern std::atomic<NimBLECharacteristic*> ssidRefreshChar;
extern std::atomic<bool> isBLEClientConnected;

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo);
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason);
};

class MyCharCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo);
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo);
};

NimBLEAdvertising* initBLE();
// BLEtick removed - now using event-driven bleSetupTask

#endif