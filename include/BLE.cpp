#include "BLE.hpp"
#include "NimBLEDevice.h"
#include "WiFi.hpp"

bool flag_scan_requested = false;

std::string tempSSID = "";
std::string tempPassword = "";
bool SSIDGiven = false;
bool passGiven = false;

std::string token = "";
bool tokenGiven = false;

// Global pointers to characteristics for notification support
NimBLECharacteristic* ssidListChar = nullptr;
NimBLECharacteristic* connectConfirmChar = nullptr;

NimBLEAdvertising* initBLE() {
  NimBLEDevice::init("BlindMaster-C6");
  
  // Optional: Boost power for better range (ESP32-C6 supports up to +20dBm)
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  // Set security
  NimBLEDevice::setSecurityAuth(false, false, true); // bonding=false, mitm=false, sc=true (Secure Connections)
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT); // No input/output capability

  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pServer->advertiseOnDisconnect(true); // Automatically restart advertising on disconnect

  NimBLEService *pService = pServer->createService("181C");

  // Create all characteristics with callbacks
  MyCharCallbacks* charCallbacks = new MyCharCallbacks();
  
  // 0x0000 - SSID List (READ + NOTIFY)
  ssidListChar = pService->createCharacteristic(
                    "0000",
                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                  );
  ssidListChar->createDescriptor("2902"); // Add BLE2902 descriptor for notifications
  
  // 0x0001 - SSID (WRITE)
  NimBLECharacteristic *ssidChar = pService->createCharacteristic(
                                      "0001",
                                      NIMBLE_PROPERTY::WRITE
                                    );
  ssidChar->setCallbacks(charCallbacks);
  
  // 0x0002 - Password (WRITE)
  NimBLECharacteristic *passChar = pService->createCharacteristic(
                                      "0002",
                                      NIMBLE_PROPERTY::WRITE
                                    );
  passChar->setCallbacks(charCallbacks);
  
  // 0x0003 - Token (WRITE)
  NimBLECharacteristic *tokenChar = pService->createCharacteristic(
                                      "0003",
                                      NIMBLE_PROPERTY::WRITE
                                    );
  tokenChar->setCallbacks(charCallbacks);
  
  // 0x0004 - SSID Refresh (WRITE)
  NimBLECharacteristic *ssidRefreshChar = pService->createCharacteristic(
                                            "0004",
                                            NIMBLE_PROPERTY::WRITE
                                          );
  ssidRefreshChar->setCallbacks(charCallbacks);
  
  // 0x0005 - Connect Confirmation (READ + NOTIFY)
  connectConfirmChar = pService->createCharacteristic(
                          "0005",
                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                        );
  connectConfirmChar->createDescriptor("2902"); // Add BLE2902 descriptor for notifications

  // Start
  pService->start();
  
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("181C");
  pAdvertising->setName("BlindMaster-C6");
  pAdvertising->enableScanResponse(true);
  pAdvertising->setPreferredParams(0x06, 0x12);  // Connection interval preferences
  pAdvertising->start();

  printf("BLE Started. Waiting...\n");
  flag_scan_requested = true;

  return pAdvertising;
}

void BLEtick(NimBLEAdvertising* pAdvertising) {
  if(flag_scan_requested) {
    flag_scan_requested = false;
    printf("Scanning WiFi...\n");
    scanAndUpdateSSIDList();
  }
}