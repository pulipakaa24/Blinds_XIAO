#include "BLE.hpp"
#include "NimBLEDevice.h"
#include "WiFi.hpp"

std::atomic<bool> flag_scan_requested{false};
std::atomic<bool> credsGiven{false};
std::mutex dataMutex;

wifi_auth_mode_t auth;
static std::string SSID = "";
static std::string TOKEN = "";
static std::string PASS = "";
static std::string UNAME = "";

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
  
  // 0x0001 - Credentials JSON (WRITE) - Replaces separate SSID/Password/Token
  // Expected JSON format: {"ssid":"network","password":"pass","token":"optional"}
  NimBLECharacteristic *credsChar = pService->createCharacteristic(
                                      "0001",
                                      NIMBLE_PROPERTY::WRITE
                                    );
  credsChar->setCallbacks(charCallbacks);
  
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

bool BLEtick(NimBLEAdvertising* pAdvertising) {
  if(flag_scan_requested) {
    flag_scan_requested = false;
    printf("Scanning WiFi...\n");
    scanAndUpdateSSIDList();
  }
  else if (credsGiven) {
    credsGiven = false;
    std::string tmpSSID;
    std::string tmpUNAME;
    std::string tmpPASS;
    wifi_auth_mode_t tmpAUTH;
    {
      std::lock_guard<std::mutex> lock(dataMutex);
      tmpSSID = SSID;
      tmpUNAME = UNAME;
      tmpPASS = PASS;
      tmpAUTH = auth;
    }

    bool wifiConnect;
    if (tmpAUTH == WIFI_AUTH_WPA2_ENTERPRISE || tmpAUTH == WIFI_AUTH_WPA3_ENTERPRISE)
      wifiConnect = bmWiFi.attemptConnect(tmpSSID.c_str(), tmpUNAME.c_str(), tmpPASS.c_str(), tmpAUTH);
    else wifiConnect = bmWiFi.attemptConnect(tmpSSID.c_str(), tmpPASS.c_str(), tmpAUTH);
    if (!wifiConnect) return false;

    // save wifi credentials here
    // Authenticate with server here
  }
  return false;
}

void MyCharCallbacks::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    std::string val = pChar->getValue();
    std::string uuidStr = pChar->getUUID().toString();
    
    printf("onWrite called! UUID: %s, Value length: %d\n", uuidStr.c_str(), val.length());
    
    // Check which characteristic was written to
    if (uuidStr.find("0001") != std::string::npos) {
      // Credentials JSON characteristic
      if (val.length() > 0) {
        printf("Received JSON: %s\n", val.c_str());
        
        // Parse JSON using cJSON
        cJSON *root = cJSON_Parse(val.c_str());
        if (root != NULL) {
          cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
          cJSON *password = cJSON_GetObjectItem(root, "password");
          cJSON *token = cJSON_GetObjectItem(root, "token");
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
            credsGiven = false;
            cJSON_Delete(root);
            return;
          }
          
          bool ssidPresent = cJSON_IsString(ssid) && ssid->valuestring != NULL;
          bool passPresent = cJSON_IsString(password) && password->valuestring != NULL;
          bool unamePresent = cJSON_IsString(uname) && uname->valuestring != NULL;
          bool tokenPresent = cJSON_IsString(token) && token->valuestring != NULL;
          bool tempCredsGiven = tokenPresent && ssidPresent && (passPresent || open) &&
                        (unamePresent || !enterprise);
          
          if (tempCredsGiven) {
            printf("Received credentials, will attempt connection\n");
            std::lock_guard<std::mutex> lock(dataMutex);

            auth = (wifi_auth_mode_t)(authType->valueint);
            SSID = ssid->valuestring;
            TOKEN = token->valuestring;
            PASS = passPresent ? password->valuestring : "";
            UNAME = unamePresent ? uname->valuestring : "";
            credsGiven = tempCredsGiven; // update the global flag.
          }
          else printf("ERROR: Did not receive necessary credentials.\n");
          cJSON_Delete(root);
        } else {
          printf("Failed to parse JSON\n");
          credsGiven = false;
        }
      }
    }
    else if (uuidStr.find("0004") != std::string::npos) {
      // Refresh characteristic
      printf("Refresh Requested\n");
      flag_scan_requested = true;
    }
    else printf("Unknown UUID: %s\n", uuidStr.c_str());
  }