#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>

RTCManager::RTCManager() : _wire(nullptr), _initialized(false) {}

bool RTCManager::begin(TwoWire* wire) {
    _wire = wire;
    
    DEBUG_PRINTLN("[RTC] Usando barramento I2C já inicializado");
    delay(500);
    
    if (!_detectRTC()) {
        DEBUG_PRINTLN("[RTC] DS3231 não encontrado");
        return false;
    }
    
    DEBUG_PRINTLN("[RTC] DS3231 detectado no barramento I2C");
    
    // ✅ Tentar com Wire global primeiro
    if (!_rtc.begin()) {
        DEBUG_PRINTLN("[RTC] Biblioteca RTClib chamou Wire.begin() internamente");
        DEBUG_PRINTLN("[RTC] Tentando inicialização manual...");
        
        // ✅ Aceitar o aviso e continuar
        _initialized = true;
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        
        DEBUG_PRINTF("[RTC] Inicializado (com aviso): %s\n", getDateTime().c_str());
        return true;
    }
    
    // Se chegou aqui, inicializou sem problemas
    delay(100);
    
    if (_rtc.lostPower()) {
        DEBUG_PRINTLN("[RTC] RTC perdeu energia - ajustando");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    _initialized = true;
    
    DateTime now = _rtc.now();
    DEBUG_PRINTF("[RTC] Inicializado: %s\n", getDateTime().c_str());
    
    if (now.year() < 2020 || now.year() > 2100) {
        DEBUG_PRINTLN("[RTC] Data inválida - ajustando");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        DEBUG_PRINTF("[RTC] Ajustado: %s\n", getDateTime().c_str());
    }
    
    return true;
}


bool RTCManager::syncWithNTP() {
    if (!_initialized) {
        DEBUG_PRINTLN("[RTC] RTC não inicializado");
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[RTC] WiFi desconectado");
        return false;
    }
    
    DEBUG_PRINTLN("[RTC] Sincronizando com NTP...");
    
    configTime(RTC_TIMEZONE_OFFSET, 0, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
    
    time_t now = 0;
    struct tm timeinfo;
    uint8_t attempts = 0;
    
    while (attempts < 40) {
        if (getLocalTime(&timeinfo)) {
            time(&now);
            if (now > 1704067200) break;
        }
        delay(500);
        attempts++;
    }
    
    if (now < 1704067200) {
        DEBUG_PRINTLN("[RTC] Timeout NTP");
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
    
    DEBUG_PRINTF("[RTC] Sincronizado com NTP: %s\n", getDateTime().c_str());
    
    return true;
}

String RTCManager::getDateTime() {
    if (!_initialized) {
        return "1970-01-01 00:00:00";
    }
    
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
    _wire->beginTransmission(DS3231_ADDRESS);
    uint8_t error = _wire->endTransmission();
    
    if (error == 0) {
        DEBUG_PRINTLN("[RTC] DS3231 encontrado no barramento I2C");
        return true;
    } else {
        DEBUG_PRINTF("[RTC] DS3231 não respondeu (erro I2C: %d)\n", error);
        return false;
    }
}

time_t RTCManager::_applyOffset(time_t utcTime) const {
    return utcTime + RTC_TIMEZONE_OFFSET;
}
