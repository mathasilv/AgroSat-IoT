#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>

RTCManager::RTCManager(HAL::I2C& i2c)
    : _rtc(),
      _i2c(&i2c),
      _initialized(false) {}

// Usa HAL::I2C para detecção; RTClib continua usando Wire por baixo do HAL
bool RTCManager::begin() {
    if (!_i2c) {
        DEBUG_PRINTLN("[RTC] Erro: HAL I2C nulo");
        return false;
    }

    DEBUG_PRINTLN("[RTC] Usando barramento I2C via HAL");

    // Delay maior para estabilização do barramento
    delay(1000);
    
    // Tentar detectar RTC até 3 vezes
    bool rtcDetected = false;
    for (uint8_t attempt = 1; attempt <= 3; attempt++) {
        DEBUG_PRINTF("[RTC] Tentativa %d/3 de deteccao...\n", attempt);
        
        if (_detectRTC()) {
            rtcDetected = true;
            DEBUG_PRINTLN("[RTC] DS3231 detectado no barramento I2C");
            break;
        }
        
        if (attempt < 3) {
            delay(500);
        }
    }
    
    if (!rtcDetected) {
        DEBUG_PRINTLN("[RTC] DS3231 nao encontrado apos 3 tentativas");
        return false;
    }
    
    // Delay adicional antes de inicializar
    delay(500);
    
    // Tentar inicializar com RTClib (usa Wire configurado pelo HAL)
    if (!_rtc.begin()) {
        DEBUG_PRINTLN("[RTC] Falha ao inicializar biblioteca RTClib");
        DEBUG_PRINTLN("[RTC] Tentando inicializacao manual...");
        
        // Forçar inicialização manual
        _initialized = true;
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        
        DEBUG_PRINTF("[RTC] Inicializado manualmente: %s\n", getDateTime().c_str());
        return true;
    }
    
    // Inicialização bem-sucedida
    delay(100);
    
    // Verificar perda de energia
    if (_rtc.lostPower()) {
        DEBUG_PRINTLN("[RTC] AVISO: RTC perdeu energia - bateria fraca?");
        DEBUG_PRINTLN("[RTC] Ajustando para data de compilacao");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    _initialized = true;
    
    DateTime now = _rtc.now();
    DEBUG_PRINTF("[RTC] Inicializado com sucesso: %s\n", getDateTime().c_str());
    
    // Validar data
    if (now.year() < 2020 || now.year() > 2100) {
        DEBUG_PRINTLN("[RTC] Data invalida detectada");
        DEBUG_PRINTF("[RTC] Ano recebido: %d\n", now.year());
        DEBUG_PRINTLN("[RTC] Ajustando para data de compilacao");
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        DEBUG_PRINTF("[RTC] Ajustado para: %s\n", getDateTime().c_str());
    }
    
    return true;
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
    
    DEBUG_PRINTLN("[RTC] Sincronizando com servidor NTP...");
    
    configTime(RTC_TIMEZONE_OFFSET, 0, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);
    
    time_t now = 0;
    struct tm timeinfo;
    uint8_t attempts = 0;
    const uint8_t MAX_ATTEMPTS = 40;
    
    while (attempts < MAX_ATTEMPTS) {
        if (getLocalTime(&timeinfo)) {
            time(&now);
            // Validar timestamp (apos 1 Jan 2024)
            if (now > 1704067200) {
                break;
            }
        }
        delay(500);
        attempts++;
        
        // Log progresso a cada 5 tentativas
        if (attempts % 5 == 0) {
            DEBUG_PRINTF("[RTC] Aguardando NTP... tentativa %d/%d\n", attempts, MAX_ATTEMPTS);
        }
    }
    
    if (now < 1704067200) {
        DEBUG_PRINTLN("[RTC] Timeout ao aguardar sincronizacao NTP (20 segundos)");
        return false;
    }
    
    // Converter para UTC
    struct tm timeinfoUTC;
    gmtime_r(&now, &timeinfoUTC);
    
    DateTime ntpTime(timeinfoUTC.tm_year + 1900, 
                     timeinfoUTC.tm_mon + 1, 
                     timeinfoUTC.tm_mday,
                     timeinfoUTC.tm_hour, 
                     timeinfoUTC.tm_min, 
                     timeinfoUTC.tm_sec);
    
    // Salvar no RTC
    _rtc.adjust(ntpTime);
    
    DEBUG_PRINTF("[RTC] Sincronizado com sucesso: %s\n", getDateTime().c_str());
    DEBUG_PRINTF("[RTC] Unix timestamp: %lu\n", getUnixTime());
    
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

// Detecção via HAL: lê registrador de segundos (0x00) e valida faixa
bool RTCManager::_detectRTC() {
    if (!_i2c) return false;

    // Ler registrador de segundos (0x00)
    uint8_t secondsReg = _i2c->readRegister(DS3231_ADDRESS, 0x00);
    uint8_t seconds = secondsReg & 0x7F;  // BCD, bit7 = CH

    if (seconds <= 59) {
        DEBUG_PRINTLN("[RTC] DS3231 respondeu corretamente (registro de segundos valido)");
        return true;
    } else {
        DEBUG_PRINTF("[RTC] Leitura invalida do registro de segundos: 0x%02X\n", secondsReg);
        return false;
    }
}

time_t RTCManager::_applyOffset(time_t utcTime) const {
    return utcTime + RTC_TIMEZONE_OFFSET;
}
