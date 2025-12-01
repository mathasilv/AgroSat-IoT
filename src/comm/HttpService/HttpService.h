#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>

class HttpService {
public:
    HttpService();

    // Envia JSON para o endpoint configurado em config.h
    // Retorna true se HTTP 200/201 e "sucesso" na resposta
    bool postJson(const String& jsonPayload);

private:
    bool _checkResponse(const String& response);
};

#endif // HTTP_SERVICE_H
