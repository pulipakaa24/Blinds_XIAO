#ifndef BMHTTP
#define BMHTTP
#include <string>
#include "cJSON.h"

extern std::string webToken;

bool httpGET(std::string endpoint, std::string token, cJSON* &JSONresponse);

#endif