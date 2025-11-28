#include "EnvService.h"
#include <string.h>   // memcpy
#include <math.h>     // fabsf, powf

EnvService::EnvService(BMP280Hal& bmp, SI7021Hal& si7021, HAL::I2C& i2c)
    : _i2c(&i2c),
      _bmp(bmp),
      _si(si7021),
      _bmpOnline(false),
      _siOnline(false),
      _temperature(NAN),
      _temperatureBMP(NAN),
      _temperatureSI(NAN),
      _pressure(NAN),
      _altitude(NAN),
      _humidity(NAN),
      _seaLevelPressure(1013.25f),
      _bmpTempValid(false),
      _siTempValid(false),
      _bmpTempFailures(0),
      _siTempFailures(0),
      _bmpFailCount(0),
      _warmupStartTime(0),
      _lastUpdateTime(0),
      _historyIndex(0),
      _historyFull(false),
      _lastPressureRead(0.0f),
      _identicalReadings(0),
      _lastSiRead(0)
{
    for (int i = 0; i < 5; i++) {
        _pressureHistory[i] = 1013.25f;
        _altitudeHistory[i] = 0.0f;
        _tempHistory[i]     = 20.0f;
    }
}

bool EnvService::begin() {
    DEBUG_PRINTLN("[EnvService] Inicializando ambiente (BMP280 + SI7021)...");

    _bmpOnline = _initBMP280();
    _siOnline  = _initSI7021();

    _updateTemperatureRedundancy();

    DEBUG_PRINTF("[EnvService] BMP280: %s, SI7021: %s\n",
                 _bmpOnline ? "ONLINE" : "offline",
                 _siOnline  ? "ONLINE" : "offline");

    return _bmpOnline || _siOnline;
}

void EnvService::update() {
    _updateBMP280();
    _updateSI7021();
    _updateTemperatureRedundancy();
}

// ====================== BMP280 ======================

bool EnvService::_waitForBMP280Measurement() {
    if (!_i2c || !_bmpOnline) return false;

    const uint8_t BMP280_STATUS_REG = 0xF3;
    const uint8_t STATUS_MEASURING  = 0x08;
    const uint8_t BMP280_ADDR       = BMP280_ADDR_1; // assume 0x76/0x77 selecionado no begin()

    uint8_t maxRetries = 50;  // ~50 ms de timeout

    while (maxRetries--) {
        uint8_t status = _i2c->readRegister(BMP280_ADDR, BMP280_STATUS_REG);
        if ((status & STATUS_MEASURING) == 0) {
            return true;
        }
        delay(1);
    }

    return false;
}

void EnvService::_updateBMP280() {
    if (!_bmpOnline) {
        _temperatureBMP = NAN;
        return;
    }

    bool readSuccess = false;
    float temp = NAN, press = NAN, alt = NAN;

    for (int retry = 0; retry < 3; retry++) {
        if (!_waitForBMP280Measurement()) {
            delay(10);
            continue;
        }

        temp  = _bmp.readTemperature();
        press = _bmp.readPressure() / 100.0f;   // Pa → hPa
        alt   = _calculateAltitude(press);

        if (!isnan(temp) && !isnan(press) && !isnan(alt)) {
            readSuccess = true;
            break;
        }

        if (retry < 2) {
            DEBUG_PRINTF("[EnvService] BMP280: Retry %d (NaN detectado)\n", retry + 1);
            delay(50);
        }
    }

    if (!readSuccess) {
        _bmpFailCount++;
        DEBUG_PRINTLN("[EnvService] BMP280: Falha após 3 tentativas");

        if (_bmpFailCount >= 5) {
            unsigned long now = millis();

            if (now - _lastUpdateTime > 30000) {
                DEBUG_PRINTLN("[EnvService] BMP280: 5 falhas, solicitando reinicialização...");
                // A reinicialização em si deve ser feita por quem chama (ex: SensorManager)
            }
        }
        return;
    }

    float tempBackup  = _temperatureBMP;
    float pressBackup = _pressure;
    float altBackup   = _altitude;

    _temperatureBMP = temp;
    _pressure       = press;
    _altitude       = alt;

    if (!_validateBMP280Reading()) {
        _temperatureBMP = tempBackup;
        _pressure       = pressBackup;
        _altitude       = altBackup;

        _bmpFailCount++;
        _bmpTempFailures++;

        DEBUG_PRINTF("[EnvService] BMP280: Leitura rejeitada (P=%.0f vs anterior %.0f hPa)\n",
                     press, pressBackup);

        bool bigDifference = fabsf(press - pressBackup) > 50.0f;
        bool sensorFrozen  = (_identicalReadings >= 10);

        if (bigDifference || sensorFrozen) {
            unsigned long now = millis();

            if (now - _lastUpdateTime > 10000) {
                if (sensorFrozen) {
                    DEBUG_PRINTLN("[EnvService] BMP280: SENSOR TRAVADO! Reinicialização recomendada.");
                } else {
                    DEBUG_PRINTLN("[EnvService] BMP280: Diferença grande detectada, reinicialização recomendada.");
                }
            }
        }
        return;
    }

    _bmpTempValid    = true;
    _bmpFailCount    = 0;
    _bmpTempFailures = 0;
}

float EnvService::_getMedian(float* values, uint8_t count) {
    float sorted[5];
    memcpy(sorted, values, count * sizeof(float));

    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                float temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }

    return sorted[count / 2];
}

bool EnvService::_isOutlier(float value, float* history, uint8_t count) {
    if (!_historyFull && _historyIndex < 3) {
        return false;
    }

    float median = _getMedian(history, count);

    float deviations[5];
    for (uint8_t i = 0; i < count; i++) {
        deviations[i] = fabsf(history[i] - median);
    }

    float mad = _getMedian(deviations, count);
    if (mad < 0.1f) mad = 0.1f;

    float score = fabsf(value - median) / mad;

    return (score > 3.0f);
}

bool EnvService::_validateBMP280Reading() {
    if (!_bmpOnline) return false;

    float temp = _temperatureBMP;
    float press = _pressure;
    float alt = _altitude;

    if (isnan(temp) || isnan(press) || isnan(alt)) {
        return false;
    }

    bool exactlyIdentical = (memcmp(&press, &_lastPressureRead, sizeof(float)) == 0);

    if (exactlyIdentical && _lastPressureRead != 0.0f) {
        _identicalReadings++;

        if (_identicalReadings >= 50) {
            DEBUG_PRINTF("[EnvService] BMP280: TRAVADO! (P=%.2f hPa por %d leituras EXATAS)\n",
                         press, _identicalReadings);
            _identicalReadings = 0;
            return false;
        }
    } else {
        _identicalReadings = 0;
    }
    _lastPressureRead = press;

    unsigned long now = millis();
    float deltaTime = (now - _lastUpdateTime) / 1000.0f;

    if (_lastUpdateTime > 0 && deltaTime > 0.1f && deltaTime < 10.0f) {
        float pressRate = fabsf(press - _pressureHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (pressRate > 20.0f) {
            DEBUG_PRINTF("[EnvService] BMP280: Taxa pressão anormal: %.1f hPa/s\n", pressRate);
            return false;
        }

        float altRate = fabsf(alt - _altitudeHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (altRate > 150.0f) {
            DEBUG_PRINTF("[EnvService] BMP280: Taxa altitude anormal: %.1f m/s\n", altRate);
            return false;
        }

        float tempRate = fabsf(temp - _tempHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (tempRate > 0.1f) {
            DEBUG_PRINTF("[EnvService] BMP280: Taxa temp anormal: %.2f°C/s\n", tempRate);
            return false;
        }
    }

    if (_warmupStartTime == 0) {
        _warmupStartTime = millis();
    }

    unsigned long warmupElapsed = millis() - _warmupStartTime;

    if (warmupElapsed < 30000) {
        DEBUG_PRINTF("[EnvService] BMP280: Warm-up (%lus/30s)\n", warmupElapsed / 1000);
    } else {
        if (_historyFull || _historyIndex >= 3) {
            uint8_t histCount = _historyFull ? 5 : _historyIndex;

            if (_isOutlier(press, _pressureHistory, histCount)) {
                DEBUG_PRINTF("[EnvService] BMP280: Pressão outlier: %.0f hPa\n", press);
                return false;
            }
        }
    }

    // Atualizar histórico
    _pressureHistory[_historyIndex] = press;
    _altitudeHistory[_historyIndex] = alt;
    _tempHistory[_historyIndex]     = temp;

    _historyIndex = (_historyIndex + 1) % 5;
    if (_historyIndex == 0) _historyFull = true;

    _lastUpdateTime = millis();

    return true;
}

bool EnvService::_initBMP280() {
    DEBUG_PRINTLN("[EnvService] ========================================");
    DEBUG_PRINTLN("[EnvService] Inicializando BMP280 (método robusto)");
    DEBUG_PRINTLN("[EnvService] ========================================");

    _bmpOnline        = false;
    _bmpTempValid     = false;
    _warmupStartTime  = 0;
    _identicalReadings= 0;
    _lastPressureRead = 0.0f;

    delay(50);

    uint8_t addresses[] = {BMP280_ADDR_1, BMP280_ADDR_2};
    bool found = false;

    for (uint8_t addr : addresses) {
        DEBUG_PRINTF("[EnvService] Tentando BMP280 em 0x%02X...\n", addr);

        for (int attempt = 0; attempt < 5; attempt++) {
            if (_bmp.begin(addr)) {
                found = true;
                DEBUG_PRINTF("[EnvService] BMP280 detectado em 0x%02X (tentativa %d)\n",
                             addr, attempt + 1);
                break;
            }
            delay(200);
        }
        if (found) break;
    }

    if (!found) {
        DEBUG_PRINTLN("[EnvService] BMP280 não detectado");
        return false;
    }

    DEBUG_PRINTLN("[EnvService] Configuração aplicada (via BMP280Hal)");
    DEBUG_PRINTLN("[EnvService] Aguardando estabilização (5 segundos)...");
    delay(5000);

    DEBUG_PRINTLN("[EnvService] Testando leituras (5 ciclos com retry)...");

    for (int i = 0; i < 5; i++) {
        bool readSuccess = false;
        float press = NAN, temp = NAN;

        for (int retry = 0; retry < 3; retry++) {
            if (_waitForBMP280Measurement()) {
                press = _bmp.readPressure() / 100.0f;
                temp  = _bmp.readTemperature();

                if (!isnan(press) && !isnan(temp)) {
                    readSuccess = true;
                    break;
                }
            }
            delay(50);
        }

        if (!readSuccess) {
            DEBUG_PRINTF("[EnvService] Falha na leitura %d após 3 tentativas\n", i + 1);
            return false;
        }

        DEBUG_PRINTF("[EnvService]   Leitura %d: T=%.1f°C P=%.0f hPa\n",
                     i + 1, temp, press);

        if (i == 4) {
            float alt = _calculateAltitude(press);
            for (int j = 0; j < 5; j++) {
                _pressureHistory[j] = press;
                _altitudeHistory[j] = alt;
                _tempHistory[j]     = temp;
            }
            _historyFull = true;
        }

        delay(200);
    }

    _bmpOnline      = true;
    _bmpTempValid   = true;
    _bmpFailCount   = 0;

    DEBUG_PRINTLN("[EnvService] ========================================");
    DEBUG_PRINTLN("[EnvService] BMP280 INICIALIZADO COM SUCESSO!");
    DEBUG_PRINTLN("[EnvService] ========================================");

    return true;
}

// ====================== SI7021 ======================

bool EnvService::_initSI7021() {
    DEBUG_PRINTLN("[EnvService] Inicializando SI7021 (driver HAL)...");

    _siOnline          = false;
    _siTempValid       = false;
    _siTempFailures    = 0;

    if (!_si.begin(SI7021_ADDRESS)) {
        DEBUG_PRINTLN("[EnvService] SI7021: Não inicializado (HAL)");
        return false;
    }

    float hum  = NAN;
    float temp = NAN;

    if (_si.readHumidity(hum)) {
        _humidity = hum;
        DEBUG_PRINTF("[EnvService] SI7021: Umidade inicial = %.1f%%\n", hum);
    }

    if (_si.readTemperature(temp)) {
        if (_validateReading(temp, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
            _temperatureSI   = temp;
            _siTempValid     = true;
            _siTempFailures  = 0;
            DEBUG_PRINTF("[EnvService] SI7021: Temperatura inicial = %.2f°C\n", temp);
        }
    }

    _siOnline   = true;
    _lastSiRead = millis();
    return true;
}

void EnvService::_updateSI7021() {
    if (!_siOnline) return;

    uint32_t currentTime = millis();
    if (currentTime - _lastSiRead < SI7021_READ_INTERVAL) return;

    bool humiditySuccess = false;

    float hum = NAN;
    if (_si.readHumidity(hum)) {
        if (!isnan(hum) && hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
            _humidity   = hum;
            _lastSiRead = currentTime;
            humiditySuccess = true;
        }
    }

    if (!humiditySuccess) {
        static uint8_t failCount = 0;
        failCount++;

        if (failCount >= 10) {
            DEBUG_PRINTLN("[EnvService] SI7021: 10 falhas consecutivas (umidade)");
            failCount = 0;
        }
        return;
    }

    float temp = NAN;
    if (_si.readTemperature(temp)) {
        if (_validateReading(temp, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
            _temperatureSI   = temp;
            _siTempValid     = true;
            _siTempFailures  = 0;
        } else {
            _siTempValid = false;
            _siTempFailures++;

            if (_siTempFailures >= MAX_TEMP_FAILURES) {
                DEBUG_PRINTLN("[EnvService] SI7021: Temperatura com falhas consecutivas");
            }
        }
    }
}

// ====================== Comum ======================

bool EnvService::_validateReading(float value, float minValid, float maxValid) {
    if (isnan(value)) return false;
    if (value < minValid || value > maxValid) return false;
    if (value == 0.0f || value == -273.15f)  return false;
    return true;
}

float EnvService::_calculateAltitude(float pressure) {
    if (pressure <= 0.0f) return 0.0f;
    float ratio = pressure / _seaLevelPressure;
    return 44330.0f * (1.0f - powf(ratio, 0.1903f));
}

void EnvService::_updateTemperatureRedundancy() {
    if (_siOnline && _siTempValid &&
        _validateReading(_temperatureSI, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
        _temperature = _temperatureSI;
        return;
    }

    if (_bmpOnline && _bmpTempValid &&
        _validateReading(_temperatureBMP, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
        _temperature = _temperatureBMP;
        return;
    }

    _temperature = NAN;
}
