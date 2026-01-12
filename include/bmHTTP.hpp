#ifndef BMHTTP_H
#define BMHTTP_H
#include <string>
#include "cJSON.h"

extern std::string webToken;

bool httpGET(std::string endpoint, std::string token, cJSON* &JSONresponse);
bool httpPOST(std::string endpoint, std::string token, cJSON* postData, cJSON* &JSONresponse);

void deleteWiFiAndTokenDetails();

#endif