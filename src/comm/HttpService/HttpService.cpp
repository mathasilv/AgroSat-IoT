#include "comm/HttpService/HttpService.h"
#include <HTTPClient.h>
#include "config.h"

HttpService::HttpService() {
}

bool HttpService::postJson(const String& jsonPayload) {
    if (jsonPayload.length() == 0 ||
        jsonPayload == "{}" ||
        jsonPayload == "null") {
        return false;
    }

    if (jsonPayload.length() > JSON_MAX_SIZE) {
        return false;
    }

    HTTPClient http;
    String url = String("https://") + HTTP_SERVER + HTTP_ENDPOINT;

    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);
    bool success = false;

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String response = http.getString();
        success = _checkResponse(response);
    }

    http.end();
    return success;
}

bool HttpService::_checkResponse(const String& response) {
    // mesma lógica que você já tem em _sendHTTPPost()
    if (response.indexOf("sucesso") >= 0 ||
        response.indexOf("Sucesso") >= 0) {
        return true;
    }

    StaticJsonDocument<256> responseDoc;
    if (deserializeJson(responseDoc, response) == DeserializationError::Ok) {
        const char* status = responseDoc["Status"];
        if (status != nullptr) {
            return String(status).indexOf("Sucesso") >= 0;
        }
    }

    // fallback: se não conseguiu interpretar, considera OK
    return true;
}
