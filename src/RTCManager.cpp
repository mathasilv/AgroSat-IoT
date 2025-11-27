/**
 * @file RTCManager.cpp
 * @brief Implementação usando HAL::i2c()
 * @version 2.0.0
 */

#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>

RTCManager::RTCManager() : _initialized(false) {}

bool RTCManager::begin() {
    DEBUG_PRINTLN("[RTC] Inicializando com HAL I2C...");
    
    // ✅ MIGRAÇÃO: HAL I2C já inicializado pelo sistema
    HAL::i2c().begin();
    
    // Delay para estabilização
    delay(1000);
    
    bool rtcDetected = false;
    for (uint8_t attempt = 1; attempt <= 3; attempt++) {
        DEBUG_PRINTF("[RTC] Tentativa %d/3...\n", attempt);
        
        if (_detectRTC()) {
            rtcDetected = true;
            DEBUG_PRINTLN("[RTC] DS3231 detectado (HAL I2C)");
            break;
        }
        
        if (attempt < 3) delay(500);
    }
    
    if (!rtcDetected) {
        DEBUG_PRINTLN("[RTC] DS3231 não encontrado");
        return false;
    }
    
    delay(500);
    
    if (!_rtc.begin()) {
        DEBUG_PRINTLN("[RTC] Falha RTClib - inicialização manual");
        _initialized = true;
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        DEBUG_PRINTF("[RTC] Data: %s\n", getDateTime().c_str());
        return true;
    }
    
    delay(100);
    
    if (_rtc.lostPower()) {
        DEBUG_PRINTLN("[RTC] AVISO: Bateria RTC fraca");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    _initialized = true;
    
    DateTime now = _rtc.now();
    DEBUG_PRINTF("[RTC] OK: %s\n", getDateTime().c_str());
    
    if (now.year() < 2020 || now.year() > 2100) {
        DEBUG_PRINTLN("[RTC] Data inválida - ajustando");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    return true;
}

bool RTCManager::syncWithNTP() {
    if (!_initialized || WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[RTC] Pré-condições NTP falharam");
        return false;
    }
    
    DEBUG_PRINTLN("[RTC] Sincronizando NTP...");
    
    configTime(RTC_TIMEZONE_OFFSET, 0, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
    
    time_t now = 0;
    struct tm timeinfo;
    uint8_t attempts = 0;
    const uint8_t MAX_ATTEMPTS = 40;
    
    while (attempts < MAX_ATTEMPTS) {
        if (getLocalTime(&timeinfo)) {
            time(&now);
            if (now > 1704067200) break;
        }
        delay(500);
        attempts++;
        
        if (attempts % 5 == 0) {
            DEBUG_PRINTF("[RTC] NTP tentativa %d/%d\n", attempts, MAX_ATTEMPTS);
        }
    }
    
    if (now < 1704067200) {
        DEBUG_PRINTLN("[RTC] NTP timeout (20s)");
        return false;
    }
    
    struct tm timeinfoUTC;
    gmtime_r(&now, &timeinfoUTC);
    
    DateTime ntpTime(timeinfoUTC.tm_year + 1900, 
                     timeinfoUTC.tm_mon + 1, 
                     timeinfoUTC.tm_mday,
                     timeinfoUTC.tm_hour, 
                     timeinfoUTC.tm_min, 
                     timeinfoUTC.tm_sec);
    
    _rtc.adjust(ntpTime);
    
    DEBUG_PRINTF("[RTC] NTP OK: %s (Unix: %lu)\n", getDateTime().c_str(), getUnixTime());
    return true;
}

String RTCManager::getDateTime() {
    if (!_initialized) return "1970-01-01 00:00:00";
    
    time_t utcTime = _rtc.now().unixtime();
    time_t localTime = _applyOffset(utcTime);
    
    struct tm timeinfo;
    gmtime_r(&localTime, &timeinfo);
    
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
        timeinfo.tm_year + 1900, 
        timeinfo.tm_mon + 1, 
        timeinfo.tm_mday,
        timeinfo.tm_hour, 
        timeinfo.tm_min, 
        timeinfo.tm_sec);
    
    return String(buffer);
}

uint32_t RTCManager::getUnixTime() {
    if (!_initialized) return 0;
    return _rtc.now().unixtime() + RTC_TIMEZONE_OFFSET;
}

bool RTCManager::_detectRTC() {
    HAL::i2c().writeByte(DS3231_ADDRESS, 0x00);
    
    uint8_t data;
    return HAL::i2c().readRegisterByte(DS3231_ADDRESS, 0x00, &data);
}

time_t RTCManager::_applyOffset(time_t utcTime) const {
    return utcTime + RTC_TIMEZONE_OFFSET;
}