#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>
#include "config.h"  // ✅ ADICIONADO - macros DEBUG_*

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
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    _initialized = true;
    
    DateTime now = _rtc.now();
    if (now.year() < 2020 || now.year() > 2100) {
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    DEBUG_PRINTF("[RTC] OK: %s\n", getDateTime().c_str());
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
    if (!_initialized || WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    DEBUG_PRINTLN("[RTC] Sync NTP...");
    
    // ✅ FIX: Somente timezone, SEM DST offset (0)
    configTime(RTC_TIMEZONE_OFFSET, 0, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
    
    time_t now = 0;
    struct tm timeinfo;
    uint8_t attempts = 0;
    
    // Timeout 20s
    while (attempts++ < 40) {
        delay(500);
        if (getLocalTime(&timeinfo)) {
            time(&now);
            if (now > 1704067200) {  // Pós 2024
                DEBUG_PRINTF("[RTC] NTP raw: %ld (local)\n", now);
                break;
            }
        }
    }
    
    if (now < 1704067200) {
        DEBUG_PRINTLN("[RTC] NTP timeout");
        return false;
    }
    
    // ✅ FIX CRÍTICO: Converter LOCAL → UTC removendo timezone offset
    time_t utcTime = now - RTC_TIMEZONE_OFFSET;
    
    struct tm timeinfoUTC;
    gmtime_r(&utcTime, &timeinfoUTC);
    
    DateTime ntpTime(
        timeinfoUTC.tm_year + 1900,
        timeinfoUTC.tm_mon + 1,
        timeinfoUTC.tm_mday,
        timeinfoUTC.tm_hour,
        timeinfoUTC.tm_min,
        timeinfoUTC.tm_sec
    );
    
    _rtc.adjust(ntpTime);
    _lost_power = false;
    
    DEBUG_PRINTF("[RTC] NTP OK: RAW=%ld LOCAL=%ld UTC=%ld\n", now, now, utcTime);
    DEBUG_PRINTF("[RTC] RTC ajustado: %s\n", getDateTime().c_str());
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
