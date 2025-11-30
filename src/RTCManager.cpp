#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>
#include "config.h"

RTCManager::RTCManager() : _wire(nullptr), _initialized(false), _lost_power(true) {}

bool RTCManager::begin(TwoWire* wire) {
    _wire = wire;
    _wire->setTimeOut(RTC_I2C_TIMEOUT_MS);
    _wire->clearWriteError();
    delay(100);
    
    bool rtcDetected = false;
    for (uint8_t attempt = 1; attempt <= 3; attempt++) {
        DEBUG_PRINTF("[RTC] Tentativa %d/3...\n", attempt);
        if (_detectRTC()) {
            rtcDetected = true;
            DEBUG_PRINTLN("[RTC] DS3231 detectado");
            break;
        }
        if (attempt < 3) delay(500);
    }
    
    if (!rtcDetected) {
        DEBUG_PRINTLN("[RTC] DS3231 nao encontrado");
        return false;
    }
    
    delay(500);
    if (!_rtc.begin()) {
        _initialized = true;
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        DEBUG_PRINTLN("[RTC] Inicializado manualmente");
        return true;
    }
    
    _lost_power = _rtc.lostPower();
    if (_lost_power) {
        DEBUG_PRINTLN("[RTC] Bateria perdida - ajustando compile time");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    _initialized = true;
    
    DateTime now = _rtc.now();
    if (now.year() < 2020 || now.year() > 2100) {
        DEBUG_PRINTF("[RTC] Data invalida: %d\n", now.year());
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    DEBUG_PRINTF("[RTC] OK: %s (unix: %lu)\n", getDateTime().c_str(), getUnixTime());
    return true;
}

String RTCManager::getDateTime() {
    if (!_initialized) {
        strcpy(_datetime_buffer, "1970-01-01 00:00:00");
        return String(_datetime_buffer);
    }
    
    time_t utcTime = _rtc.now().unixtime();
    time_t localTime = _applyOffset(utcTime);
    
    struct tm timeinfo;
    localtime_r(&localTime, &timeinfo);
    
    snprintf(_datetime_buffer, sizeof(_datetime_buffer),
             "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    return String(_datetime_buffer);
}

bool RTCManager::syncWithNTP() {
    if (!_initialized) {
        DEBUG_PRINTLN("[RTC] RTC nao inicializado - impossivel sincronizar");
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[RTC] WiFi desconectado - impossivel sincronizar NTP");
        return false;
    }
    
    DEBUG_PRINTLN("[RTC] ========================================");
    DEBUG_PRINTLN("[RTC] SINCRONIZANDO COM NTP");
    DEBUG_PRINTF("[RTC] Servidor primario: %s\n", NTP_SERVER_PRIMARY);
    DEBUG_PRINTF("[RTC] Servidor secundario: %s\n", NTP_SERVER_SECONDARY);
    DEBUG_PRINTLN("[RTC] ========================================");
    
    // ✅ UTC puro (offset=0)
    configTime(0, 0, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
    
    time_t now = 0;
    struct tm timeinfo;
    uint8_t attempts = 0;
    const uint8_t MAX_ATTEMPTS = 40;
    
    DEBUG_PRINTLN("[RTC] Aguardando resposta NTP...");
    while (attempts < MAX_ATTEMPTS) {
        if (getLocalTime(&timeinfo)) {
            time(&now);
            if (now > 1704067200) {
                DEBUG_PRINTF("[RTC] NTP respondeu! UTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                            timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                break;
            }
        }
        delay(500);
        attempts++;
        
        if (attempts % 5 == 0) {
            DEBUG_PRINTF("[RTC] Tentativa %d/%d...\n", attempts, MAX_ATTEMPTS);
        }
    }
    
    if (now < 1704067200) {
        DEBUG_PRINTLN("[RTC] ❌ TIMEOUT NTP (20 segundos)");
        return false;
    }
    
    // Salvar UTC no RTC
    DateTime ntpTime(timeinfo.tm_year + 1900, 
                     timeinfo.tm_mon + 1, 
                     timeinfo.tm_mday,
                     timeinfo.tm_hour, 
                     timeinfo.tm_min, 
                     timeinfo.tm_sec);
    
    _rtc.adjust(ntpTime);
    _lost_power = false;
    
    DEBUG_PRINTLN("[RTC] ========================================");
    DEBUG_PRINTLN("[RTC] ✅ SINCRONIZADO COM SUCESSO!");
    DEBUG_PRINTF("[RTC] UTC armazenado: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    DEBUG_PRINTF("[RTC] Hora local (BRT): %s\n", getDateTime().c_str());
    DEBUG_PRINTF("[RTC] Unix timestamp: %lu\n", getUnixTime());
    DEBUG_PRINTLN("[RTC] ========================================");
    
    return true;
}

bool RTCManager::_detectRTC() {
    _wire->clearWriteError();
    _wire->beginTransmission(DS3231_ADDRESS);
    _wire->write(0x00);
    if (_wire->endTransmission() != 0) return false;
    
    _wire->requestFrom(static_cast<uint8_t>(DS3231_ADDRESS), static_cast<uint8_t>(1));
    return _wire->available() > 0;
}

time_t RTCManager::_applyOffset(time_t utcTime) const {
    return utcTime + RTC_TIMEZONE_OFFSET;
}
