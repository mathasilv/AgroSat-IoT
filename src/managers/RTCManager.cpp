#include "RTCManager.h"

RTCManager::RTCManager(HAL::I2C& i2c)
    : _rtc(i2c, DS3231_ADDRESS),
      _i2c(&i2c),
      _initialized(false) {}

bool RTCManager::begin() {
    if (!_i2c) {
        DEBUG_PRINTLN("[RTC] Erro: HAL I2C nulo");
        return false;
    }

    DEBUG_PRINTLN("[RTC] Usando barramento I2C via HAL");
    delay(1000);

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

    delay(500);

    if (!_rtc.begin()) {
        DEBUG_PRINTLN("[RTC] Falha ao inicializar DS3231Hal");
        return false;
    }

    // Verificar perda de energia via OSF [web:296]
    if (_rtc.lostPower()) {
        DEBUG_PRINTLN("[RTC] AVISO: RTC perdeu energia - bateria fraca?");
        DEBUG_PRINTLN("[RTC] Ajustando para data de compilacao");

        RtcDateTime compileTime = {
            2025, 1, 1, 0, 0, 0  // placeholder: vamos montar a partir de __DATE__/__TIME__ se quiser
        };
        // Aqui, para manter igual ao RTClib::DateTime(F(__DATE__), F(__TIME__)),
        // você pode reaproveitar um helper existente ou manter esse default.

        _rtc.adjust(compileTime);
        _rtc.clearLostPowerFlag();
    }

    _initialized = true;

    RtcDateTime now;
    if (_rtc.now(now)) {
        uint32_t unixNow = _rtc.unixtime(now);
        DEBUG_PRINTF("[RTC] Inicializado com sucesso: %s\n", getDateTime().c_str());

        // Validar ano
        if (now.year < 2020 || now.year > 2100) {
            DEBUG_PRINTLN("[RTC] Data invalida detectada");
            DEBUG_PRINTF("[RTC] Ano recebido: %d\n", now.year);
            DEBUG_PRINTLN("[RTC] Ajustando para data de compilacao");

            RtcDateTime compileTime = {2025, 1, 1, 0, 0, 0};
            _rtc.adjust(compileTime);
            _rtc.now(now);
            DEBUG_PRINTF("[RTC] Ajustado para: %s\n", getDateTime().c_str());
        }
    } else {
        DEBUG_PRINTLN("[RTC] Falha ao ler hora atual");
        return false;
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
            if (now > 1704067200) { // > 1 Jan 2024
                break;
            }
        }
        delay(500);
        attempts++;

        if (attempts % 5 == 0) {
            DEBUG_PRINTF("[RTC] Aguardando NTP... tentativa %d/%d\n",
                         attempts, MAX_ATTEMPTS);
        }
    }

    if (now < 1704067200) {
        DEBUG_PRINTLN("[RTC] Timeout ao aguardar sincronizacao NTP (20 segundos)");
        return false;
    }

    struct tm timeinfoUTC;
    gmtime_r(&now, &timeinfoUTC);

    RtcDateTime ntpTime {
        (uint16_t)(timeinfoUTC.tm_year + 1900),
        (uint8_t)(timeinfoUTC.tm_mon + 1),
        (uint8_t) timeinfoUTC.tm_mday,
        (uint8_t) timeinfoUTC.tm_hour,
        (uint8_t) timeinfoUTC.tm_min,
        (uint8_t) timeinfoUTC.tm_sec
    };

    _rtc.adjust(ntpTime);

    DEBUG_PRINTF("[RTC] Sincronizado com sucesso: %s\n", getDateTime().c_str());
    DEBUG_PRINTF("[RTC] Unix timestamp: %lu\n", getUnixTime());

    return true;
}

String RTCManager::getDateTime() {
    if (!_initialized) {
        return "1970-01-01 00:00:00";
    }

    RtcDateTime dt;
    if (!_rtc.now(dt)) {
        return "1970-01-01 00:00:00";
    }

    time_t utcTime   = _rtc.unixtime(dt);
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

    RtcDateTime dt;
    if (!_rtc.now(dt)) return 0;

    return _rtc.unixtime(dt) + RTC_TIMEZONE_OFFSET;
}

// Detecção via HAL: já era boa; mantemos como está
bool RTCManager::_detectRTC() {
    if (!_i2c) return false;

    uint8_t secondsReg = _i2c->readRegister(DS3231_ADDRESS, 0x00);
    uint8_t seconds    = secondsReg & 0x7F;

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
