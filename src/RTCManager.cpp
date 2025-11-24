#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>

RTCManager::RTCManager() : _wire(nullptr), _initialized(false) {}

bool RTCManager::begin(TwoWire* wire) {
    _wire = wire;
    
    DEBUG_PRINTLN("[RTC] Usando barramento I2C ja inicializado");
    
    // Limpar erros anteriores do barramento
    _wire->clearWriteError();
    
    // Delay maior para estabilizacao do barramento
    delay(1000);
    
    // Tentar detectar RTC ate 3 vezes
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
    
    // Tentar inicializar com RTClib
    if (!_rtc.begin()) {
        DEBUG_PRINTLN("[RTC] Falha ao inicializar biblioteca RTClib");
        DEBUG_PRINTLN("[RTC] Tentando inicializacao manual...");
        
        // Forcar inicializacao manual
        _initialized = true;
        _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        
        DEBUG_PRINTF("[RTC] Inicializado manualmente: %s\n", getDateTime().c_str());
        return true;
    }
    
    // Inicializacao bem-sucedida
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

bool RTCManager::_detectRTC() {
    // Limpar erros antes de tentar comunicacao
    _wire->clearWriteError();
    
    // Tentar acessar registro de segundos (0x00)
    _wire->beginTransmission(DS3231_ADDRESS);
    _wire->write(0x00);
    uint8_t error = _wire->endTransmission();
    
    if (error == 0) {
        // Tentar ler para confirmar comunicacao - cast explicito para evitar ambiguidade
        uint8_t bytesToRead = 1;
        _wire->requestFrom((uint8_t)DS3231_ADDRESS, bytesToRead);
        
        if (_wire->available()) {
            _wire->read(); // Descartar leitura
            DEBUG_PRINTLN("[RTC] DS3231 respondeu corretamente");
            return true;
        } else {
            DEBUG_PRINTLN("[RTC] DS3231 ACK mas sem dados");
            return false;
        }
    } else {
        DEBUG_PRINTF("[RTC] DS3231 nao respondeu (erro I2C: %d)\n", error);
        
        // Decodificar erro
        switch(error) {
            case 1:
                DEBUG_PRINTLN("[RTC] Erro: Dados muito longos para buffer");
                break;
            case 2:
                DEBUG_PRINTLN("[RTC] Erro: NACK ao enviar endereco");
                break;
            case 3:
                DEBUG_PRINTLN("[RTC] Erro: NACK ao enviar dados");
                break;
            case 4:
                DEBUG_PRINTLN("[RTC] Erro: Outro erro I2C");
                break;
            case 5:
                DEBUG_PRINTLN("[RTC] Erro: Timeout I2C");
                break;
            default:
                DEBUG_PRINTF("[RTC] Erro: Codigo desconhecido %d\n", error);
                break;
        }
        
        return false;
    }
}

time_t RTCManager::_applyOffset(time_t utcTime) const {
    return utcTime + RTC_TIMEZONE_OFFSET;
}
