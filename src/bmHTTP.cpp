#include "bmHTTP.hpp"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "defines.h"
#include "esp_crt_bundle.h"

std::string webToken;
const std::string urlBase = std::string("http") + (secureSrv ? "s" : "") + "://" + srvAddr + "/";

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA: {
            // Append received data to buffer (handles both chunked and non-chunked)
            if (evt->data_len > 0 && evt->user_data != NULL) {
              std::string* rxBuffer = (std::string*)evt->user_data;
              rxBuffer->append((char*)evt->data, evt->data_len);
            }
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

bool httpGET(std::string endpoint, std::string token, cJSON* &JSONresponse) {
  std::string url = urlBase + endpoint;
  std::string responseBuffer = "";
  
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.event_handler = _http_event_handler; // Attach the bucket
  config.user_data = &responseBuffer;         // Pass pointer to our string so the handler can write to it
  if (secureSrv) {
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = esp_crt_bundle_attach;
  }
  
  esp_http_client_handle_t client = esp_http_client_init(&config);
  
  // Add authorization header
  std::string authHeader = "Bearer " + token;
  esp_http_client_set_header(client, "Authorization", authHeader.c_str());
  
  // Open connection and fetch headers
  esp_err_t err = esp_http_client_perform(client);
  bool success = false;

  if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        printf("Status = %d, Content Length = %d\n", status_code, esp_http_client_get_content_length(client));
        
        if (status_code == 200) {
            printf("Response: %s\n", responseBuffer.c_str());
            JSONresponse = cJSON_Parse(responseBuffer.c_str());
            if (JSONresponse) success = true;
        }
    } else {
        printf("HTTP GET failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return success;
}

bool httpPOST(std::string endpoint, std::string token, cJSON* postData, cJSON* &JSONresponse) {
  std::string url = urlBase + endpoint;
  std::string responseBuffer = "";
  
  // Convert JSON object to string
  char* postString = cJSON_PrintUnformatted(postData);
  if (postString == NULL) {
    printf("Failed to serialize JSON for POST\n");
    return false;
  }
  
  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.method = HTTP_METHOD_POST;
  config.event_handler = _http_event_handler;
  config.user_data = &responseBuffer;
  if (secureSrv) {
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = esp_crt_bundle_attach;
  }
  
  esp_http_client_handle_t client = esp_http_client_init(&config);
  
  // Set headers
  std::string authHeader = "Bearer " + token;
  esp_http_client_set_header(client, "Authorization", authHeader.c_str());
  esp_http_client_set_header(client, "Content-Type", "application/json");
  
  // Set POST data
  esp_http_client_set_post_field(client, postString, strlen(postString));
  
  // Perform request
  esp_err_t err = esp_http_client_perform(client);
  bool success = false;

  if (err == ESP_OK) {
    int status_code = esp_http_client_get_status_code(client);
    printf("Status = %d, Content Length = %d\n", status_code, esp_http_client_get_content_length(client));
    
    if (status_code == 200 || status_code == 201) {
      printf("Response: %s\n", responseBuffer.c_str());
      JSONresponse = cJSON_Parse(responseBuffer.c_str());
      if (JSONresponse) success = true;
    }
  } else {
    printf("HTTP POST failed: %s\n", esp_err_to_name(err));
  }

  free(postString);
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