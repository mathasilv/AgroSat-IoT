/**
 * @file RTCManager.cpp
 * @brief Implementação Robusta do RTCManager
 */

#include "RTCManager.h"
#include "config.h" // ✅ Necessário para DEBUG_PRINTLN
#include <WiFi.h>
#include "time.h"

RTCManager::RTCManager() : _wire(nullptr), _initialized(false), _lost_power(true) {}

bool RTCManager::begin(TwoWire* wire) {
    _wire = wire;
    
    if (!_detectRTC()) {
        DEBUG_PRINTLN("[RTC] ERRO: DS3231 não detectado.");
        return false;
    }
    
    if (!_rtc.begin(wire)) {
        DEBUG_PRINTLN("[RTC] ERRO: Falha no driver RTC.");
        return false;
    }
    
    _initialized = true;
    _lost_power = _rtc.lostPower();

    if (_lost_power) {
        DEBUG_PRINTLN("[RTC] Bateria perdida! Ajustando tempo...");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    _syncSystemToRTC();
    
    DEBUG_PRINTF("[RTC] Online. Local: %s\n", getDateTime().c_str());
    return true;
}

void RTCManager::update() {}

bool RTCManager::syncWithNTP() {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[RTC] NTP Falhou: Sem WiFi.");
        return false;
    }

    DEBUG_PRINTLN("[RTC] Sincronizando NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

    struct tm timeinfo;
    uint32_t start = millis();
    bool success = false;
    
    // Timeout de 5s para não travar o boot
    while (millis() - start < 5000) {
        if (getLocalTime(&timeinfo, 10)) {
            success = true;
            break;
        }
        delay(100);
    }

    if (success) {
        time_t now;
        time(&now);
        struct tm* tm_local = localtime(&now);
        
        // Ajusta o RTC com o tempo obtido do NTP
        _rtc.adjust(DateTime(tm_local->tm_year + 1900, tm_local->tm_mon + 1, tm_local->tm_mday,
                             tm_local->tm_hour, tm_local->tm_min, tm_local->tm_sec));
                             
        _lost_power = false;
        DEBUG_PRINTF("[RTC] NTP OK: %s\n", getDateTime().c_str());
        return true;
    } 
    
    DEBUG_PRINTLN("[RTC] NTP Timeout.");
    return false;
}

// === Getters ===

String RTCManager::getDateTime() {
    if (!_initialized) return "2000-01-01 00:00:00";
    DateTime now = _rtc.now();
    char buf[25];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    return String(buf);
}

String RTCManager::getUTCDateTime() {
    if (!_initialized) return "2000-01-01 00:00:00";
    // RTC está em UTC-3. Para ter UTC, somamos o offset inverso (+3h)
    // Opcionalmente, pode-se usar unix time.
    uint32_t unix = _rtc.now().unixtime();
    DateTime utc(unix - GMT_OFFSET_SEC); // Subtrai o offset negativo (soma)
    
    char buf[25];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             utc.year(), utc.month(), utc.day(),
             utc.hour(), utc.minute(), utc.second());
    return String(buf);
}

uint32_t RTCManager::getUnixTime() {
    if (!_initialized) return 0;
    // Retorna Unix Time UTC
    return _rtc.now().unixtime() - GMT_OFFSET_SEC;
}

DateTime RTCManager::getNow() {
    if (!_initialized) return DateTime((uint32_t)0); // ✅ Cast explícito para resolver ambiguidade
    return _rtc.now();
}

bool RTCManager::isInitialized() const { return _initialized; }

// === Privados ===

bool RTCManager::_detectRTC() {
    _wire->beginTransmission(DS3231_ADDR);
    return (_wire->endTransmission() == 0);
}

void RTCManager::_syncSystemToRTC() {
    if (!_initialized) return;
    DateTime now = _rtc.now();
    struct timeval tv;
    tv.tv_sec = now.unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}