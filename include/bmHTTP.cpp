#include "bmHTTP.hpp"
#include "esp_http_client.h"
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
  
  esp_err_t err = esp_http_client_perform(client);
  bool success = false;

  if (err == ESP_OK) {
    int status_code = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    
    printf("HTTP Status = %d, content_length = %d\n", status_code, content_length);
    
    if (status_code == 200) {
      std::string responseData = "";
      char buffer[512]; // Read in 512-byte blocks
      int read_len;

      // Read until the server stops sending (read_len <= 0)
      while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) 
        responseData.append(buffer, read_len);

      if (!responseData.empty()) {
        JSONresponse = cJSON_Parse(responseData.c_str());
        success = (JSONresponse != NULL);
      }
    }
  } else printf("HTTP request failed: %s\n", esp_err_to_name(err));
  
  esp_http_client_cleanup(client);
  return success;
}