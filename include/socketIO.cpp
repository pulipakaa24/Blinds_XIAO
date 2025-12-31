#include "socketIO.hpp"
#include "esp_socketio_client.h"
#include "bmHTTP.hpp" // To access webToken
#include "WiFi.hpp"
#include "setup.hpp"
#include "cJSON.h"
#include "calibration.hpp"
#include "servo.hpp"

static esp_socketio_client_handle_t io_client;
static esp_socketio_packet_handle_t tx_packet = NULL;

std::atomic<bool> statusResolved{true};
std::atomic<bool> connected{false};

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
              connected = false;
              statusResolved = true;
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
                      // Report back actual calibration status from device
                      else {
                        bool deviceCalibrated = calib.getCalibrated();
                        emitCalibStatus(deviceCalibrated);
                        printf("  Reported calibrated=%d for port %d\n", deviceCalibrated, port);
                      }
                    }
                  }
                  
                  // Now mark as connected
                  connected = true;
                  statusResolved = true;
                } else {
                  printf("Device authentication failed\n");
                  connected = false;
                  statusResolved = true;
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
              connected = false;
              statusResolved = true;
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
                    if (!servoInitCalib()) emitCalibError("Initialization failed");
                    else emitCalibStage1Ready();
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
                    else emitCalibDone();
                  }
                }
              }
            }
            
            // Handle server position change (manual or scheduled)
            else if (strcmp(eventName->valuestring, "posUpdates") == 0) {
              printf("Received position update from server\n");
              cJSON *updateList = cJSON_GetArrayItem(json, 1);
              
              if (cJSON_IsArray(updateList)) {
                int updateCount = cJSON_GetArraySize(updateList);
                printf("Processing %d position update(s)\n", updateCount);
                
                for (int i = 0; i < updateCount; i++) {
                  cJSON *update = cJSON_GetArrayItem(updateList, i);
                  cJSON *periphNum = cJSON_GetObjectItem(update, "periphNum");
                  cJSON *pos = cJSON_GetObjectItem(update, "pos");
                  
                  if (periphNum && cJSON_IsNumber(periphNum) && 
                      pos && cJSON_IsNumber(pos)) {
                    int port = periphNum->valueint;
                    int position = pos->valueint;
                    
                    if (port != 1)
                      printf("ERROR: Received position update for non-1 port: %d\n", port);
                    else {
                      printf("Position update: position %d\n", position);
                      runToAppPos(position);
                    }
                  } 
                  else printf("Invalid position update format\n");
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
      
      // Check WebSocket error details
      esp_websocket_event_data_t *ws_event = data->websocket_event;
      if (ws_event && ws_event->error_handle.esp_ws_handshake_status_code != 0) {
        printf("HTTP Status: %d\n", ws_event->error_handle.esp_ws_handshake_status_code);
        if (ws_event->error_handle.esp_ws_handshake_status_code == 401 || 
          ws_event->error_handle.esp_ws_handshake_status_code == 403) {
          printf("Authentication failed - invalid token\n");
        }
      }
      
      // Set flags to indicate connection failure
      connected = false;
      statusResolved = true;
      break;
    }
  }
  
  // Handle WebSocket-level disconnections
  if (data->websocket_event_id == WEBSOCKET_EVENT_DISCONNECTED) {
    printf("WebSocket disconnected\n");
    connected = false;
    statusResolved = true;
  }
}

void initSocketIO() {
  // Prepare the Authorization Header (Bearer format)
  std::string authHeader = "Authorization: Bearer " + webToken + "\r\n";

  statusResolved = false;
  connected = false;
  
  esp_socketio_client_config_t config = {};
  config.websocket_config.uri = "ws://192.168.1.190:3000/socket.io/?EIO=4&transport=websocket";
  config.websocket_config.headers = authHeader.c_str();
  
  io_client = esp_socketio_client_init(&config);
  tx_packet = esp_socketio_client_get_tx_packet(io_client);
  
  esp_socketio_register_events(io_client, SOCKETIO_EVENT_ANY, socketio_event_handler, NULL);
  esp_socketio_client_start(io_client);
}

void stopSocketIO() {
  if (io_client != NULL) {
    printf("Stopping Socket.IO client...\n");
    esp_socketio_client_close(io_client, pdMS_TO_TICKS(1000));
    esp_socketio_client_destroy(io_client);
    io_client = NULL;
    tx_packet = NULL;
    connected = false;
    statusResolved = false;
  }
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
void emitCalibDone(int port = 1) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  emitSocketEvent("calib_done", data);
}

// Function to emit 'calib_stage1_ready' to notify server device is ready for tilt up
void emitCalibStage1Ready(int port = 1) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  emitSocketEvent("calib_stage1_ready", data);
}

// Function to emit 'calib_stage2_ready' to notify server device is ready for tilt down
void emitCalibStage2Ready(int port = 1) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  emitSocketEvent("calib_stage2_ready", data);
}

// Function to emit 'report_calib_status' to tell server device's actual calibration state
void emitCalibStatus(bool calibrated, int port = 1) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  cJSON_AddBoolToObject(data, "calibrated", calibrated);
  emitSocketEvent("report_calib_status", data);
}

// Function to emit 'device_calib_error' to notify server of calibration failure
void emitCalibError(const char* errorMessage, int port = 1) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  cJSON_AddStringToObject(data, "message", errorMessage);
  emitSocketEvent("device_calib_error", data);
}

// Function to emit 'pos_hit' to notify server of position change
void emitPosHit(int pos, int port = 1) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "port", port);
  cJSON_AddNumberToObject(data, "pos", pos);
  emitSocketEvent("pos_hit", data);
}