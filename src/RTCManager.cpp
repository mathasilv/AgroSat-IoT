#include "RTCManager.h"
#include <WiFi.h>
#include <lwip/apps/sntp.h>

RTCManager::RTCManager() : _wire(&Wire) {
    memset(&_status, 0, sizeof(RTCStatus));
}

bool RTCManager::begin() {
    DEBUG_PRINTLN("[RTCManager] Inicializando DS3231 RTC...");
    
    // Usar pinos I2C dos sensores (Wire já está inicializado pelo TelemetryManager)
    // Não chamar begin() aqui se Wire já foi inicializado
    
    if (!_detectRTC()) {
        DEBUG_PRINTLN("[RTCManager] ERRO: DS3231 não encontrado no I2C!");
        return false;
    }
    
    DEBUG_PRINTLN("[RTCManager] DS3231 detectado com sucesso");
    
    if (!_rtc.begin(_wire)) {
        DEBUG_PRINTLN("[RTCManager] ERRO: Falha ao inicializar RTC");
        return false;
    }
    
    DateTime now = _rtc.now();
    DEBUG_PRINTF("[RTCManager] RTC funcionando: %s\n", now.timestamp().c_str());
    
    // Verificar se tempo é válido (ano > 2020)
    _status.timeValid = (now.year() >= 2020);
    
    if (!_status.timeValid) {
        DEBUG_PRINTLN("[RTCManager] AVISO: Tempo do RTC fora da faixa válida");
    }
    
    // Sincronizar relógio do sistema
    _syncSystemClock(now);
    
    // Obter temperatura
    _status.temperature = getTemperature();
    DEBUG_PRINTF("[RTCManager] Temperatura RTC: %.2f°C\n", _status.temperature);
    
    // Incrementar boot count na EEPROM
    uint32_t bootCount = 0;
    if (eepromRead(EEPROM_ADDR_BOOT_COUNT, (uint8_t*)&bootCount, 4)) {
        bootCount++;
    } else {
        DEBUG_PRINTLN("[RTCManager] ERRO: Falha ao ler boot count da EEPROM");
        bootCount = 1;
    }
    
    if (!eepromWrite(EEPROM_ADDR_BOOT_COUNT, (uint8_t*)&bootCount, 4)) {
        DEBUG_PRINTLN("[RTCManager] ERRO: Falha ao salvar boot count na EEPROM");
    }
    
    _status.bootCount = bootCount;
    DEBUG_PRINTF("[RTCManager] Boot count: %d\n", bootCount);
    
    _status.initialized = true;
    DEBUG_PRINTLN("[RTCManager] Inicialização concluída com sucesso");
    
    return true;
}

bool RTCManager::syncWithNTP(const char* ssid, const char* password) {
    DEBUG_PRINTLN("[RTCManager] Sincronizando com servidor NTP...");
    
    // Verificar se WiFi já está conectado (conectado pelo CommunicationManager)
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[RTCManager] WiFi não conectado");
        return false;
    }
    
    delay(1000);  // Aguardar WiFi estabilizar
    
    // Parar qualquer sincronização SNTP anterior
    sntp_stop();
    delay(100);
    
    // Configurar timezone Brasil (GMT-3)
    DEBUG_PRINTLN("[RTCManager] Configurando timezone Brasil (GMT-3)...");
    setenv("TZ", TIMEZONE_STRING, 1);
    tzset();
    
    // Iniciar SNTP
    DEBUG_PRINTLN("[RTCManager] Iniciando SNTP...");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)NTP_SERVER_PRIMARY);
    sntp_setservername(1, (char*)NTP_SERVER_SECONDARY);
    sntp_setservername(2, (char*)NTP_SERVER_TERTIARY);
    sntp_init();
    
    DEBUG_PRINTLN("[RTCManager] Aguardando sincronização SNTP...");
    
    uint8_t attempts = 0;
    time_t now = 0;
    
    // Aguardar timestamp válido do NTP (não usar cache)
    while (attempts < 20) {
        time(&now);
        
        // Validar se timestamp é atual (> 01/01/2024 = 1704067200)
        if (now > 1704067200) {
            DEBUG_PRINTF("[RTCManager] ✓ SNTP sincronizado! Timestamp: %ld\n", now);
            break;
        }
        
        delay(1000);
        attempts++;
        DEBUG_PRINTF("[RTCManager] Aguardando SNTP... %d/20 (ts=%ld)\n", attempts, now);
    }
    
    if (now < 1704067200) {
        DEBUG_PRINTLN("[RTCManager] ERRO: Timeout aguardando NTP");
        sntp_stop();
        return false;
    }
    
    // Obter struct tm do horário local
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        DEBUG_PRINTLN("[RTCManager] ERRO: getLocalTime falhou");
        sntp_stop();
        return false;
    }
    
    DEBUG_PRINTF("[RTCManager] Data/Hora: %04d-%02d-%02d %02d:%02d:%02d (Brasília)\n",
        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    // Atualizar RTC com timestamp NTP
    DateTime ntpTime(now);
    _rtc.adjust(ntpTime);
    _syncSystemClock(ntpTime);
    
    _status.ntpSynced = true;
    _status.timeValid = true;
    _status.lastSyncTime = millis();
    
    DEBUG_PRINTLN("[RTCManager] ✓ RTC sincronizado!");
    
    return true;
}

DateTime RTCManager::getDateTime() {
    if (!_status.initialized) return DateTime((uint32_t)0);
    return _rtc.now();
}

float RTCManager::getTemperature() {
    if (!_status.initialized) return NAN;
    return _rtc.getTemperature();
}

void RTCManager::update() {
    if (!_status.initialized) return;
    _status.temperature = getTemperature();
}

uint32_t RTCManager::getUnixTime() {
    if (!_status.initialized) return 0;
    return _rtc.now().unixtime();
}

String RTCManager::getISO8601() {
    if (!_status.initialized) return "1970-01-01T00:00:00Z";
    
    DateTime now = _rtc.now();
    char buffer[25];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
        now.year(), now.month(), now.day(),
        now.hour(), now.minute(), now.second());
    return String(buffer);
}

String RTCManager::getTimeString() {
    if (!_status.initialized) return "00:00:00";
    
    DateTime now = _rtc.now();
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
        now.hour(), now.minute(), now.second());
    return String(buffer);
}

String RTCManager::getDateString() {
    if (!_status.initialized) return "1970-01-01";
    
    DateTime now = _rtc.now();
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
        now.year(), now.month(), now.day());
    return String(buffer);
}

bool RTCManager::_detectRTC() {
    _wire->beginTransmission(0x68);
    return (_wire->endTransmission() == 0);
}

void RTCManager::_syncSystemClock(const DateTime& dt) {
    struct timeval tv;
    tv.tv_sec = dt.unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    DEBUG_PRINTLN("[RTCManager] Relógio do sistema sincronizado com RTC");
}

bool RTCManager::eepromWrite(uint16_t addr, const uint8_t* data, size_t len) {
    _wire->beginTransmission(EEPROM_I2C_ADDR);
    _wire->write((uint8_t)(addr >> 8));
    _wire->write((uint8_t)(addr & 0xFF));
    
    for (size_t i = 0; i < len; i++) {
        _wire->write(data[i]);
    }
    
    uint8_t error = _wire->endTransmission();
    if (error != 0) {
        DEBUG_PRINTF("[RTCManager] ERRO I2C na escrita EEPROM: %d\n", error);
        return false;
    }
    
    delay(10);  // Aguardar EEPROM processar escrita
    return true;
}

bool RTCManager::eepromRead(uint16_t addr, uint8_t* data, size_t len) {
    // Posicionar ponteiro de leitura
    _wire->beginTransmission(EEPROM_I2C_ADDR);
    _wire->write((uint8_t)(addr >> 8));
    _wire->write((uint8_t)(addr & 0xFF));
    
    if (_wire->endTransmission() != 0) {
        DEBUG_PRINTLN("[RTCManager] ERRO: Falha ao posicionar ponteiro EEPROM");
        return false;
    }
    
    // Ler dados
    _wire->requestFrom(EEPROM_I2C_ADDR, len);
    
    for (size_t i = 0; i < len; i++) {
        if (_wire->available()) {
            data[i] = _wire->read();
        } else {
            DEBUG_PRINTLN("[RTCManager] ERRO: Dados insuficientes na EEPROM");
            return false;
        }
    }
    
    return true;
}
