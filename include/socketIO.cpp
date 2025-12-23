#include "socketIO.hpp"
#include "esp_socketio_client.h"
#include "bmHTTP.hpp" // To access webToken
#include "WiFi.hpp"
#include "setup.hpp"
#include "cJSON.h"

static esp_socketio_client_handle_t io_client;
static esp_socketio_packet_handle_t tx_packet = NULL;

std::atomic<bool> statusResolved{false};
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
                printf("Connected to default namespace\n");
            }
            connected = true;
            statusResolved = true;
            break;
        }
            
        case SOCKETIO_EVENT_DATA: {
            printf("Received Socket.IO data\n");
            // Parse the received packet
            cJSON *json = esp_socketio_packet_get_json(packet);
            if (json) {
                char *json_str = cJSON_Print(json);
                printf("Data: %s\n", json_str);
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
            
            // Disconnect and enter setup mode
            printf("Disconnecting and entering setup mode...\n");
            esp_socketio_client_close(io_client, pdMS_TO_TICKS(1000));

            esp_socketio_client_destroy(io_client);
            initialSetup();
            connected = false;
            statusResolved = true;
            break;
        }
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