#include "bmHTTP.hpp"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "defines.h"
#define httpSrv "http://192.168.1.190:3000/"

std::string webToken;

bool httpGET(std::string endpoint, std::string token, cJSON* &JSONresponse) {
  std::string url = std::string(httpSrv) + endpoint;
  
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.method = HTTP_METHOD_GET;
  
  esp_http_client_handle_t client = esp_http_client_init(&config);
  
  // Add authorization header
  std::string authHeader = "Bearer " + token;
  esp_http_client_set_header(client, "Authorization", authHeader.c_str());
  
  // Open connection and fetch headers
  esp_err_t err = esp_http_client_open(client, 0);
  bool success = false;

  if (err == ESP_OK) {
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    printf("HTTP Status = %d, content_length = %d\n", status_code, content_length);
    
    if (status_code == 200 && content_length > 0) {
      // Allocate buffer for the response
      char *buffer = (char *)malloc(content_length + 1);
      if (buffer) {
        int total_read = 0;
        int read_len;
        
        // Read the response body
        while (total_read < content_length) {
          read_len = esp_http_client_read(client, buffer + total_read, content_length - total_read);
          if (read_len <= 0) break;
          total_read += read_len;
        }
        
        buffer[total_read] = '\0';
        printf("Response body: %s\n", buffer);
        
        JSONresponse = cJSON_Parse(buffer);
        success = (JSONresponse != NULL);
        
        if (!success) {
          printf("Failed to parse JSON\n");
        }
        
        free(buffer);
      } else {
        printf("Failed to allocate buffer for response\n");
      }
    }
    
    esp_http_client_close(client);
  } else printf("HTTP request failed: %s\n", esp_err_to_name(err));
  
  esp_http_client_cleanup(client);
  return success;
}

void deleteWiFiAndTokenDetails() {
  nvs_handle_t wifiHandle;
  if (nvs_open(nvsWiFi, NVS_READWRITE, &wifiHandle) == ESP_OK) {
    if (nvs_erase_all(wifiHandle) == ESP_OK) {
      printf("Successfully erased WiFi details\n");
      nvs_commit(wifiHandle);
    }
    else printf("ERROR: Erase wifi failed\n");
    nvs_close(wifiHandle);
  }
  else printf("ERROR: Failed to open WiFi section for deletion\n");
  
  nvs_handle_t authHandle;
  if (nvs_open(nvsAuth, NVS_READWRITE, &authHandle) == ESP_OK) {
    if (nvs_erase_all(authHandle) == ESP_OK) {
      printf("Successfully erased Auth details\n");
      nvs_commit(authHandle);
    }
    else printf("ERROR: Erase auth failed\n");
    nvs_close(authHandle);
  }
  else printf("ERROR: Failed to open Auth section for deletion\n");
}