#ifndef BLE_H
#define BLE_H

#include "NimBLEDevice.h"

extern bool flag_scan_requested;

extern std::string SSID;
extern std::string password;
extern bool SSIDGiven;
extern bool passGiven;

extern std::string token;
extern bool tokenGiven;

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
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    std::string val = pChar->getValue();
    std::string uuidStr = pChar->getUUID().toString();
    
    printf("onWrite called! UUID: %s, Value length: %d\n", uuidStr.c_str(), val.length());
    
    // Check which characteristic was written to
    if (uuidStr.find("0001") != std::string::npos) {
      // SSID characteristic
      if (val.length() > 0) {
        printf("Received SSID: %s\n", val.c_str());
        SSID = val;
        SSIDGiven = true;
      }
    }
    else if (uuidStr.find("0002") != std::string::npos) {
      // Password characteristic
      if (val.length() > 0) {
        printf("Received Password: %s\n", val.c_str());
        passGiven = true;
        password = val;
      }
    }
    else if (uuidStr.find("0003") != std::string::npos) {
      // Token characteristic
      if (val.length() > 0) {
        printf("Received Token: %s\n", val.c_str());
        tokenGiven = true;
        token = val;
      }
    }
    else if (uuidStr.find("0004") != std::string::npos) {
      // Refresh characteristic
      printf("Refresh Requested\n");
      flag_scan_requested = true;
    }
    else {
      printf("Unknown UUID: %s\n", uuidStr.c_str());
    }
  }
};

NimBLEAdvertising* initBLE();
void BLEtick(NimBLEAdvertising* pAdvertising);

#endif