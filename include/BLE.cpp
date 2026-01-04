#include "BLE.hpp"
#include "NimBLEDevice.h"
#include "WiFi.hpp"
#include "nvs_flash.h"
#include "socketIO.hpp"
#include "defines.h"
#include <mutex>
#include "bmHTTP.hpp"

std::atomic<bool> isBLEClientConnected{false};
std::atomic<bool> scanBlock{false};
std::atomic<bool> finalAuth{false};
std::mutex dataMutex;

wifi_auth_mode_t auth;
static std::string SSID = "";
static std::string TOKEN = "";
static std::string PASS = "";
static std::string UNAME = "";

// Global pointers to characteristics for notification support
std::atomic<NimBLECharacteristic*> ssidListChar = nullptr;
std::atomic<NimBLECharacteristic*> connectConfirmChar = nullptr;
std::atomic<NimBLECharacteristic*> authConfirmChar = nullptr;
std::atomic<NimBLECharacteristic*> credsChar = nullptr;
std::atomic<NimBLECharacteristic*> tokenChar = nullptr;
std::atomic<NimBLECharacteristic*> ssidRefreshChar = nullptr;

NimBLEAdvertising* initBLE() {
  finalAuth = false;
  
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
  
  // 0x0000 - SSID List (READ)
  ssidListChar = pService->createCharacteristic(
                    "0000",
                    NIMBLE_PROPERTY::READ
                  );
  ssidListChar.load()->createDescriptor("2902"); // Add BLE2902 descriptor for notifications
  
  // 0x0001 - Credentials JSON (WRITE) - Replaces separate SSID/Password/Uname
  // Expected JSON format: {"ssid":"network","password":"pass"}
  credsChar = pService->createCharacteristic(
                                      "0001",
                                      NIMBLE_PROPERTY::WRITE
                                    );
  credsChar.load()->setCallbacks(charCallbacks);

  // 0x0002 - Token (WRITE)
  tokenChar = pService->createCharacteristic(
                                      "0002",
                                      NIMBLE_PROPERTY::WRITE
                                    );
  tokenChar.load()->setCallbacks(charCallbacks);

  // 0x0003 - Auth Confirmation (READ + NOTIFY)
  authConfirmChar = pService->createCharacteristic(
                          "0003",
                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                        );
  authConfirmChar.load()->createDescriptor("2902"); // Add BLE2902 descriptor for notifications
  
  // 0x0004 - SSID Refresh
  ssidRefreshChar = pService->createCharacteristic(
                                            "0004",
                                            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                                          );
  ssidRefreshChar.load()->setCallbacks(charCallbacks);
  
  // 0x0005 - Connect Confirmation (READ + NOTIFY)
  connectConfirmChar = pService->createCharacteristic(
                          "0005",
                          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                        );
  connectConfirmChar.load()->createDescriptor("2902"); // Add BLE2902 descriptor for notifications

  // Start
  pService->start();
  
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("181C");
  pAdvertising->setName("BlindMaster-C6");
  pAdvertising->enableScanResponse(true);
  pAdvertising->setPreferredParams(0x06, 0x12);  // Connection interval preferences
  pAdvertising->start();

  printf("BLE Started. Waiting...\n");

  return pAdvertising;
}

void notifyConnectionStatus(bool success) {
  NimBLECharacteristic* tmpConfChar = connectConfirmChar.load();
  tmpConfChar->setValue(success ? "Connected" : "Error");
  tmpConfChar->notify();
  tmpConfChar->setValue("");  // Clear value after notify
}
void notifyAuthStatus(bool success) {
  NimBLECharacteristic* tmpConfChar = authConfirmChar.load();
  tmpConfChar->setValue(success ? "Authenticated" : "Error");
  tmpConfChar->notify();
  tmpConfChar->setValue("");  // Clear value after notify
}

// BLEtick() removed - replaced by bleSetupTask() in setup.cpp

void reset() {
  esp_wifi_scan_stop();
  if (!finalAuth) esp_wifi_disconnect();
  scanBlock = false;
}

void MyServerCallbacks::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
  isBLEClientConnected = true;
  printf("Client connected\n");
  reset();
};

void MyServerCallbacks::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
  isBLEClientConnected = false;
  printf("Client disconnected - reason: %d\n", reason);
  reset();
}

void MyCharCallbacks::onRead(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
  printf("Characteristic Read\n");
}

void MyCharCallbacks::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    std::string val = pChar->getValue();
    std::string uuidStr = pChar->getUUID().toString();
    
    printf("onWrite called! UUID: %s, Value length: %d\n", uuidStr.c_str(), val.length());
    
    // Load atomic pointers for comparison
    NimBLECharacteristic* currentCredsChar = credsChar.load();
    NimBLECharacteristic* currentTokenChar = tokenChar.load();
    NimBLECharacteristic* currentRefreshChar = ssidRefreshChar.load();
    
    // Check which characteristic was written to
    if (pChar == currentCredsChar) {
      // Credentials JSON characteristic
      if (val.length() > 0) {
        printf("Received JSON: %s\n", val.c_str());
        
        // Parse JSON using cJSON
        cJSON *root = cJSON_Parse(val.c_str());
        if (root != NULL) {
          cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
          cJSON *password = cJSON_GetObjectItem(root, "password");
          cJSON *authType = cJSON_GetObjectItem(root, "auth");
          cJSON *uname = cJSON_GetObjectItem(root, "uname");
          
          bool enterprise = false;
          bool open = false;
          bool error = false;
          if (cJSON_IsNumber(authType)) {
            enterprise = authType->valueint == WIFI_AUTH_WPA2_ENTERPRISE || 
              authType->valueint == WIFI_AUTH_WPA3_ENTERPRISE;
            open = authType->valueint == WIFI_AUTH_OPEN;
            error = authType->valueint < 0 || authType->valueint >= WIFI_AUTH_MAX;
          }
          else error = true;
          if (error) {
            printf("ERROR: Invalid Auth mode passed in with JSON.\n");
            cJSON_Delete(root);
            return;
          }
          
          bool ssidPresent = cJSON_IsString(ssid) && ssid->valuestring != NULL;
          bool passPresent = cJSON_IsString(password) && password->valuestring != NULL;
          bool unamePresent = cJSON_IsString(uname) && uname->valuestring != NULL;
          bool tempCredsGiven = ssidPresent && (passPresent || open) &&
                        (unamePresent || !enterprise);
          
          if (tempCredsGiven) {
            printf("Received credentials, will attempt connection\n");
            std::lock_guard<std::mutex> lock(dataMutex);

            auth = (wifi_auth_mode_t)(authType->valueint);
            SSID = ssid->valuestring;
            PASS = passPresent ? password->valuestring : "";
            UNAME = unamePresent ? uname->valuestring : "";
            // Signal via event group instead of flag
            xEventGroupSetBits(g_system_events, EVENT_BLE_CREDS_RECEIVED);
          }
          else printf("ERROR: Did not receive necessary credentials.\n");
          cJSON_Delete(root);
        } else {
          printf("Failed to parse JSON\n");
        }
      }
    }
    else if (pChar == currentTokenChar) {
      if (val.length() > 0) {
        printf("Received Token: %s\n", val.c_str());
        std::lock_guard<std::mutex> lock(dataMutex);
        TOKEN = val;
        // Signal via event group instead of flag
        xEventGroupSetBits(g_system_events, EVENT_BLE_TOKEN_RECEIVED);
      }
    }
    else if (pChar == currentRefreshChar) {
      if (val == "Start") {
        // Refresh characteristic
        printf("Refresh Requested\n");
        // Signal via event group instead of flag
        xEventGroupSetBits(g_system_events, EVENT_BLE_SCAN_REQUEST);
      }
      else if (val == "Done") {
        printf("Data read complete\n");
        scanBlock = false;
      }
    }
    else printf("Unknown UUID: %s\n", uuidStr.c_str());
  }