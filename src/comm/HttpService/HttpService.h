#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>

class HttpService {
public:
    HttpService();
    // Retorna true se enviou com sucesso (HTTP 200/201)
    bool postJson(const String& jsonPayload);
};

#endif