#include "socketIO.hpp"
#include "esp_socketio_client.h"
#include "bmHTTP.hpp" // To access webToken
#include "WiFi.hpp"
#include "setup.hpp"
#include "cJSON.h"
#include "calibration.hpp"
#include "servo.hpp"
#include "defines.h"
#include "esp_crt_bundle.h"

static esp_socketio_client_handle_t io_client;
static esp_socketio_packet_handle_t tx_packet = NULL;
static bool stopSocketFlag = false;
std::atomic<bool> socketIOactive{false};

// Event handler for Socket.IO events
static void socketio_event_handler(void *handler_args, esp_event_base_t base, 
                                 int32_t event_id, void *event_data) {
  esp_socketio_event_data_t *data = (esp_socketio_event_data_t *)event_data;
  esp_socketio_packet_handle_t packet = data->socketio_packet;

  switch (event_id) {
    case SOCKETIO_EVENT_OPENED:
      printf("Socket.IO Received OPEN packet\n");
      // Connect to default namespace "/"
      esp_socketio_client_connect_nsp(data->client, NULL, NULL);
      break;
        
    case SOCKETIO_EVENT_NS_CONNECTED: {
      printf("Socket.IO Connected to namespace!\n");
      // Check if connected to default namespace
      char *nsp = esp_socketio_packet_get_nsp(packet);
      if (strcmp(nsp, "/") == 0) {
        printf("Connected to default namespace - waiting for device_init...\n");
      }
      // Don't set connected yet - wait for device_init message from server
      break;
    }
        
    case SOCKETIO_EVENT_DATA: {
      printf("Received Socket.IO data\n");
      // Parse the received packet
      cJSON *json = esp_socketio_packet_get_json(packet);
      if (json) {
        char *json_str = cJSON_Print(json);
        printf("Data: %s\n", json_str);
        
        // Check if this is an array event
        if (cJSON_IsArray(json) && cJSON_GetArraySize(json) >= 2) {
          cJSON *eventName = cJSON_GetArrayItem(json, 0);
          
          if (cJSON_IsString(eventName)) {
            // Handle error event
            if (strcmp(eventName->valuestring, "error") == 0) {
              printf("Received error message from server\n");
              cJSON *data = cJSON_GetArrayItem(json, 1);
              
              if (data) {
                cJSON *message = cJSON_GetObjectItem(data, "message");
                if (message && cJSON_IsString(message)) {
                  printf("Server error: %s\n", message->valuestring);
                }
              }
              
              // Mark connection as failed
              if (calibTaskHandle != NULL)
                xTaskNotify(calibTaskHandle, false, eSetValueWithOverwrite);
            }
            // Handle device_init event
            else if (strcmp(eventName->valuestring, "device_init") == 0) {
              printf("Received device_init message\n");
              cJSON *data = cJSON_GetArrayItem(json, 1);
              
              if (data) {
                cJSON *type = cJSON_GetObjectItem(data, "type");
                if (type && strcmp(type->valuestring, "success") == 0) {
                  printf("Device authenticated successfully\n");
                  
                  // Parse device state
                  cJSON *deviceState = cJSON_GetObjectItem(data, "deviceState");
                  if (cJSON_IsArray(deviceState)) {
                    int stateCount = cJSON_GetArraySize(deviceState);
                    printf("Device has %d peripheral(s):\n", stateCount);
                    
                    for (int i = 0; i < stateCount; i++) {
                      cJSON *periph = cJSON_GetArrayItem(deviceState, i);
                      int port = cJSON_GetObjectItem(periph, "port")->valueint;
                      int lastPos = cJSON_GetObjectItem(periph, "lastPos")->valueint;
                      // TODO: UPDATE MOTOR/ENCODER STATES BASED ON THIS, as well as the successive websocket updates.
                      printf("  Port %d: pos=%d\n", port, lastPos);
                      if (port != 1) printf("ERROR: NON-1 PORT RECEIVED\n");
                    }
                  }
                  
                  // Now mark as connected
                  if (calibTaskHandle != NULL)
                    xTaskNotify(calibTaskHandle, true, eSetValueWithOverwrite);
                } else {
                  printf("Device authentication failed\n");
                  Calibration::clearCalibrated();
                  deleteWiFiAndTokenDetails();
                  if (calibTaskHandle != NULL)
                    xTaskNotify(calibTaskHandle, false, eSetValueWithOverwrite);
                }
              }
            }
            // Handle device_deleted event
            else if (strcmp(eventName->valuestring, "device_deleted") == 0) {
              printf("Device has been deleted from account - disconnecting\n");
              cJSON *data = cJSON_GetArrayItem(json, 1);
              if (data) {
                cJSON *message = cJSON_GetObjectItem(data, "message");
                if (message && cJSON_IsString(message)) {
                  printf("Server message: %s\n", message->valuestring);
                }
              }
              Calibration::clearCalibrated();
              deleteWiFiAndTokenDetails();
              if (calibTaskHandle != NULL)
                xTaskNotify(calibTaskHandle, false, eSetValueWithOverwrite);
            }

            // Handle calib_start event
            else if (strcmp(eventName->valuestring, "calib_start") == 0) {
              printf("Device calibration begun, setting up...\n");
              cJSON *data = cJSON_GetArrayItem(json, 1);
              if (data) {
                cJSON *port = cJSON_GetObjectItem(data, "port");
                if (port && cJSON_IsNumber(port)) {
                  if (port->valueint != 1) {
                    printf("Error, non-1 port received for calibration\n");
                    emitCalibError("Non-1 Port");
                  }
                  else {
                    printf("Running initCalib...\n");
                    if (!servoInitCalib()) {
                      printf("initCalib returned False\n");
                      emitCalibError("Initialization failed");
                    }
                    else {
                      printf("Ready to calibrate\n");
                      emitCalibStage1Ready();
                    }
                  }
                }
              }
            }
            
            // Handle user_stage1_complete event
            else if (strcmp(eventName->valuestring, "user_stage1_complete") == 0) {
              printf("User completed stage 1 (tilt up), switching direction...\n");
              cJSON *data = cJSON_GetArrayItem(json, 1);
              if (data) {
                cJSON *port = cJSON_GetObjectItem(data, "port");
                if (port && cJSON_IsNumber(port)) {
                  if (port->valueint != 1) {
                    printf("Error, non-1 port received for calibration\n");
                    emitCalibError("Non-1 Port");
                  }
                  else {
                    if (!servoBeginDownwardCalib())
                      emitCalibError("Direction Switch Failed");
                    else emitCalibStage2Ready();
                  }
                }
              }
            }
            
            // Handle user_stage2_complete event
            else if (strcmp(eventName->valuestring, "user_stage2_complete") == 0) {
              printf("User completed stage 2 (tilt down), finalizing calibration...\n");
              cJSON *data = cJSON_GetArrayItem(json, 1);
              if (data) {
                cJSON *port = cJSON_GetObjectItem(data, "port");
                if (port && cJSON_IsNumber(port)) {
                  if (port->valueint != 1) {
                    printf("Error, non-1 port received for calibration\n");
                    emitCalibError("Non-1 port");
                  }
                  else {
                    if (!servoCompleteCalib()) emitCalibError("Completion failed");
                    else {
                      emitCalibDone();
                    }
                  }
                }
              }
            }
            
            // Handle calib_done_ack event
            else if (strcmp(eventName->valuestring, "calib_done_ack") == 0) {
              printf("Server acknowledged calibration completion - safe to disconnect\n");
              stopSocketFlag = true;
            }

            // Handle cancel_calib event
            else if (strcmp(eventName->valuestring, "cancel_calib") == 0) {
              printf("Canceling calibration process...\n");
              cJSON *data = cJSON_GetArrayItem(json, 1);
              if (data) {
                cJSON *port = cJSON_GetObjectItem(data, "port");
                if (port && cJSON_IsNumber(port)) {
                  if (port->valueint != 1) {
                    printf("Error, non-1 port received for calibration\n");
                    emitCalibError("Non-1 Port");
                  }
                  else {
                    servoCancelCalib();
                  }
                }
              }
            }
          }
        }
        
        free(json_str);
      }
      break;
    }
        
    case SOCKETIO_EVENT_ERROR: {
      printf("Socket.IO Error!\n");
      servoCancelCalib();
      esp_websocket_event_data_t *ws_event = data->websocket_event;
      
      if (ws_event) {
          // 1. Check for TLS/SSL specific errors (Certificate issues)
          if (ws_event->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
              
              // This prints the "MbedTLS" error code (The low-level crypto library)
              // Common codes: -0x2700 (CRT verify failed), -0x7200 (SSL handshake failed)
              if (ws_event->error_handle.esp_tls_stack_err != 0) {
                  printf("TLS/SSL Stack Error: -0x%x\n", -ws_event->error_handle.esp_tls_stack_err);
              }

              // 2. Check the Certificate Verification Flags
              // If this is non-zero, the certificate was rejected.
              if (ws_event->error_handle.esp_tls_cert_verify_flags != 0) {
                  uint32_t flags = ws_event->error_handle.esp_tls_cert_verify_flags;
                  printf("Certificate Verification FAILED. Flags: 0x%lx\n", flags);
                  
                  // Simple decoder for common flags:
                  if (flags & (1 << 0)) printf(" - CRT_NOT_TRUSTED (Root CA not found in bundle)\n");
                  if (flags & (1 << 1)) printf(" - CRT_BAD_KEY_USAGE\n");
                  if (flags & (1 << 2)) printf(" - CRT_EXPIRED (Check your ESP32 system time!)\n");
                  if (flags & (1 << 3)) printf(" - CRT_CN_MISMATCH (Domain name doesn't match cert)\n");
              }
          }
          
          // 3. Check for HTTP Handshake errors (401/403/404)
          // This happens if SSL worked, but the server rejected your path or token
          else if (ws_event->error_handle.error_type == WEBSOCKET_ERROR_TYPE_HANDSHAKE) {
              int status = ws_event->error_handle.esp_ws_handshake_status_code;
              printf("HTTP Handshake Error: %d\n", status);
              
              if (status == 401 || status == 403) {
                  printf("Authentication failed - invalid token\n");
              } else if (status == 404) {
                  printf("404 Not Found - Check your URI/Path\n");
              }
          }
      }
      
      if (calibTaskHandle != NULL)
        xTaskNotify(calibTaskHandle, false, eSetValueWithOverwrite);
      break;
    }
  }
  
  // Handle WebSocket-level disconnections
  if (data->websocket_event_id == WEBSOCKET_EVENT_DISCONNECTED) {
    printf("WebSocket disconnected\n");
    if (calibTaskHandle != NULL)
      xTaskNotify(calibTaskHandle, false, eSetValueWithOverwrite);
  }
  if (stopSocketFlag) {
    if (calibTaskHandle != NULL)
      xTaskNotify(calibTaskHandle, 2, eSetValueWithOverwrite);
    stopSocketFlag = false; // Clear flag after notifying once
  }
}

const std::string uriString = std::string("ws") + (secureSrv ? "s" : "") + "://" + srvAddr + "/socket.io/?EIO=4&transport=websocket";
void initSocketIO() {
  stopSocketFlag = false; // Reset flag for new connection
  // Prepare the Authorization Header (Bearer format)
  std::string authHeader = "Authorization: Bearer " + webToken + "\r\n";
  
  esp_socketio_client_config_t config = {};
  config.websocket_config.uri = uriString.c_str();
  config.websocket_config.headers = authHeader.c_str();

  if (secureSrv) {
      config.websocket_config.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
      config.websocket_config.crt_bundle_attach = esp_crt_bundle_attach;
  }
  
  io_client = esp_socketio_client_init(&config);
  tx_packet = esp_socketio_client_get_tx_packet(io_client);
  
  esp_socketio_register_events(io_client, SOCKETIO_EVENT_ANY, socketio_event_handler, NULL);
  esp_socketio_client_start(io_client);

  socketIOactive = true;
}

void stopSocketIO() {
  if (io_client != NULL) {
    printf("Stopping Socket.IO client...\n");
    esp_socketio_client_close(io_client, pdMS_TO_TICKS(1000));
    esp_socketio_client_destroy(io_client);
    io_client = NULL;
    tx_packet = NULL;
  }
  socketIOactive = false;
}

// Helper function to emit Socket.IO event with data
static void emitSocketEvent(const char* eventName, cJSON* data) {
  if (esp_socketio_packet_set_header(tx_packet, EIO_PACKET_TYPE_MESSAGE, 
                                      SIO_PACKET_TYPE_EVENT, NULL, -1) == ESP_OK) {
    cJSON *array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateString(eventName));
    cJSON_AddItemToArray(array, data);
    
    esp_socketio_packet_set_json(tx_packet, array);
    esp_socketio_client_send_data(io_client, tx_packet);
    esp_socketio_packet_reset(tx_packet);
    
    cJSON_Delete(array);
  } else {
    // If packet header setup failed, clean up the data object
    cJSON_Delete(data);
  }
}

// Function to emit 'calib_done' as expected by your server
void emitCalibDone(int port) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  emitSocketEvent("calib_done", data);
}

// Function to emit 'calib_stage1_ready' to notify server device is ready for tilt up
void emitCalibStage1Ready(int port) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  emitSocketEvent("calib_stage1_ready", data);
}

// Function to emit 'calib_stage2_ready' to notify server device is ready for tilt down
void emitCalibStage2Ready(int port) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  emitSocketEvent("calib_stage2_ready", data);
}

// Function to emit 'report_calib_status' to tell server device's actual calibration state
void emitCalibStatus(bool calibrated, int port) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  cJSON_AddBoolToObject(data, "calibrated", calibrated);
  emitSocketEvent("report_calib_status", data);
}

// Function to emit 'device_calib_error' to notify server of calibration failure
void emitCalibError(const char* errorMessage, int port) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  cJSON_AddStringToObject(data, "message", errorMessage);
  emitSocketEvent("device_calib_error", data);
}

// Function to emit 'pos_hit' to notify server of position change
void emitPosHit(int pos, int port) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  cJSON_AddNumberToObject(data, "pos", pos);
  emitSocketEvent("pos_hit", data);
}