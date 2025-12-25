#include "socketIO.hpp"
#include "esp_socketio_client.h"
#include "bmHTTP.hpp" // To access webToken
#include "WiFi.hpp"
#include "setup.hpp"
#include "cJSON.h"

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
                                            bool awaitCalib = cJSON_IsTrue(cJSON_GetObjectItem(periph, "awaitCalib"));
                                            bool calibrated = cJSON_IsTrue(cJSON_GetObjectItem(periph, "calibrated"));
                                            // TODO: UPDATE MOTOR/ENCODER STATES BASED ON THIS, as well as the successive websocket updates.
                                            printf("  Port %d: pos=%d, calibrated=%d, awaitCalib=%d\n",
                                                   port, lastPos, calibrated, awaitCalib);
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

// Function to emit 'calib_done' as expected by your server
void emitCalibDone(int port) {
    // Set packet header: EIO MESSAGE type, SIO EVENT type, default namespace "/"
    if (esp_socketio_packet_set_header(tx_packet, EIO_PACKET_TYPE_MESSAGE, 
                                       SIO_PACKET_TYPE_EVENT, NULL, -1) == ESP_OK) {
        // Create JSON array with event name and data
        cJSON *array = cJSON_CreateArray();
        cJSON_AddItemToArray(array, cJSON_CreateString("calib_done"));
        
        cJSON *data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "port", port);
        cJSON_AddItemToArray(array, data);
        
        // Set the JSON payload
        esp_socketio_packet_set_json(tx_packet, array);
        
        // Send the packet
        esp_socketio_client_send_data(io_client, tx_packet);
        
        // Reset packet for reuse
        esp_socketio_packet_reset(tx_packet);
        
        cJSON_Delete(array);
    }
}