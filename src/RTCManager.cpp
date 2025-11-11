#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>

RTCManager::RTCManager() : _wire(nullptr), _initialized(false) {}

bool RTCManager::begin(TwoWire* wire) {
    _wire = wire;
    
    // Verificar se Wire já foi inicializado
    if (_wire == &Wire) {
        _wire->beginTransmission(0x00);
        if (_wire->endTransmission() == 4) {
            _wire->begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
            _wire->setClock(100000);
            delay(100);
        }
    }
    
    delay(200);
    
    // Detectar DS3231
    if (!_detectRTC()) {
        DEBUG_PRINTLN("[RTC] DS3231 não encontrado");
        return false;
    }
    
    // Inicializar RTC
    if (!_rtc.begin(_wire)) {
        DEBUG_PRINTLN("[RTC] Falha ao inicializar");
        return false;
    }
    
    delay(100);
    
    // Verificar perda de energia
    if (_rtc.lostPower()) {
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    _initialized = true;
    
    DateTime now = _rtc.now();
    DEBUG_PRINTF("[RTC] Inicializado: %s\n", getDateTime().c_str());
    
    return true;
}

bool RTCManager::syncWithNTP() {
    if (!_initialized) return false;
    
    // Verificar WiFi
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[RTC] WiFi desconectado");
        return false;
    }
    
    DEBUG_PRINTLN("[RTC] Sincronizando com NTP...");
    
    // Configurar timezone
    configTime(RTC_TIMEZONE_OFFSET, 0, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
    
    // Aguardar sincronização
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
    
    // Salvar no RTC (sempre em UTC)
    struct tm timeinfoUTC;
    gmtime_r(&now, &timeinfoUTC);
    
    DateTime ntpTime(timeinfoUTC.tm_year + 1900, timeinfoUTC.tm_mon + 1, timeinfoUTC.tm_mday,
                     timeinfoUTC.tm_hour, timeinfoUTC.tm_min, timeinfoUTC.tm_sec);
    
    _rtc.adjust(ntpTime);
    
    DEBUG_PRINTF("[RTC] Sincronizado: %s\n", getDateTime().c_str());
    
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
        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    return String(buffer);
}

uint32_t RTCManager::getUnixTime() {
    if (!_initialized) return 0;
    return _rtc.now().unixtime() + RTC_TIMEZONE_OFFSET;
}

bool RTCManager::_detectRTC() {
    _wire->beginTransmission(0x68);
    return (_wire->endTransmission() == 0);
}

time_t RTCManager::_applyOffset(time_t utcTime) const {
    return utcTime + RTC_TIMEZONE_OFFSET;
}
