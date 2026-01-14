/**
 * @file HttpService.cpp
 * @brief Implementação do serviço HTTP para envio de telemetria
 */

#include "comm/HttpService/HttpService.h"
#include <HTTPClient.h>
#include "config.h"

HttpService::HttpService() {}

bool HttpService::postJson(const String& jsonPayload) {
    if (jsonPayload.length() == 0) return false;

    HTTPClient http;
    String url = String("https://") + HTTP_SERVER + HTTP_ENDPOINT;

    // Configuração segura
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);
    bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

    if (success) {
        // Verificação extra da resposta (opcional, mas boa prática)
        String response = http.getString();
        if (response.indexOf("erro") >= 0 || response.indexOf("Error") >= 0) {
            success = false;
        }
    } else {
        DEBUG_PRINTF("[HTTP] Erro %d: %s\n", httpCode, http.errorToString(httpCode).c_str());
    }

    http.end();
    return success;
}