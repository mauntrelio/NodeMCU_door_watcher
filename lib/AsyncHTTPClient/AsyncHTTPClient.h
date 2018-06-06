#include <Arduino.h>
#include <ESPAsyncTCP.h>

void sendHTTP(String requestBody, char * responseBody, const char * httpHost);
