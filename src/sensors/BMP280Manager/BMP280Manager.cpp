/**
 * @file BMP280Manager.cpp
 * @brief Implementação do driver BMP280 com validação e detecção de anomalias
 */

#include "BMP280Manager.h"
#include <math.h>

BMP280Manager::BMP280Manager() 
    : _bmp280(Wire),
      _temperature(NAN), _pressure(NAN), _altitude(NAN),
      _online(false), _tempValid(false),
      _failCount(0),
      _lastReinitTime(0), _warmupStartTime(0),
      _historyIndex(0), _historyFull(false), _lastUpdateTime(0),
      _lastPressureRead(0.0f), _identicalReadings(0)
{
    _initHistoryValues();
}

void BMP280Manager::_initHistoryValues() {
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        _pressureHistory[i] = 1013.25f;
        _altitudeHistory[i] = 0.0f;
        _tempHistory[i] = 20.0f;
    }
}

bool BMP280Manager::begin() {
    DEBUG_PRINTLN("[BMP280Manager] Inicializando...");
    
    _online = false;
    _tempValid = false;
    _failCount = 0;
    _identicalReadings = 0;
    _initHistoryValues();
    
    uint8_t addresses[] = {BMP280::I2C_ADDR_PRIMARY, BMP280::I2C_ADDR_SECONDARY};
    bool detected = false;
    
    for (uint8_t addr : addresses) {
        if (_bmp280.begin(addr)) {
            DEBUG_PRINTF("[BMP280Manager] Sensor detectado em 0x%02X\n", addr);
            detected = true;
            break;
        }
    }
    
    if (!detected) {
        DEBUG_PRINTLN("[BMP280Manager] ERRO: Sensor nao encontrado.");
        return false;
    }
    
    if (!_bmp280.configure(
        BMP280::Mode::NORMAL,
        BMP280::TempOversampling::X1,
        BMP280::PressOversampling::X8,
        BMP280::Filter::OFF,
        BMP280::StandbyTime::MS_125
    )) {
        DEBUG_PRINTLN("[BMP280Manager] ERRO: Falha na configuracao.");
        return false;
    }
    
    delay(100); 
    float t, p, a;
    if (_readRaw(t, p, a)) {
        _online = true;
        _tempValid = true;
        _warmupStartTime = millis();
        for(int i=0; i<HISTORY_SIZE; i++) _updateHistory(t, p, a);
        DEBUG_PRINTLN("[BMP280Manager] Inicializado com sucesso.");
        return true;
    }

    DEBUG_PRINTLN("[BMP280Manager] Aviso: Sensor detectado mas leitura falhou.");
    return false;
}

void BMP280Manager::update() {
    if (!_online) return;
    
    float temp, press, alt;
    bool readSuccess = false;
    
    for (int i = 0; i < 3; i++) {
        if (_readRaw(temp, press, alt)) {
            readSuccess = true;
            break;
        }
        delay(10);
    }
    
    if (!readSuccess) {
        _failCount++;
        if (_failCount >= 10 && _canReinit()) {
            DEBUG_PRINTLN("[BMP280Manager] Falhas excessivas. Reinicializando...");
            forceReinit();
        }
        return;
    }

    if (!_tempValid) {
        _temperature = temp;
        _pressure = press;
        _altitude = alt;
        _tempValid = true;
        _failCount = 0;
        
        for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
            _pressureHistory[i] = press;
            _altitudeHistory[i] = alt;
            _tempHistory[i] = temp;
        }
        _historyFull = true;
        _lastUpdateTime = millis();
        DEBUG_PRINTLN("[BMP280Manager] Historico reiniciado.");
        return;
    }
    
    if (!_validateReading(temp, press, alt)) {
        _failCount++;
        if ((_failCount >= 20) && _canReinit()) {
            DEBUG_PRINTLN("[BMP280Manager] Dados invalidos persistentes. Resetando...");
            forceReinit();
        }
        if (_identicalReadings >= 500 && _canReinit()) {
             DEBUG_PRINTLN("[BMP280Manager] Aviso: Leitura estatica.");
             _identicalReadings = 0;
        }
        return;
    }
    
    _temperature = temp;
    _pressure = press;
    _altitude = alt;
    _tempValid = true;
    _failCount = 0;
    _updateHistory(temp, press, alt);
}

void BMP280Manager::reset() {
    _bmp280.reset();
    _online = false;
    _tempValid = false;
    _failCount = 0;
    delay(100);
}

void BMP280Manager::forceReinit() {
    _lastReinitTime = millis();
    begin();
}

bool BMP280Manager::_readRaw(float& temp, float& press, float& alt) {
    temp = _bmp280.readTemperature();
    press = _bmp280.readPressure() / 100.0f; // Pa -> hPa
    alt = _bmp280.readAltitude();
    return (!isnan(temp) && !isnan(press) && !isnan(alt));
}

bool BMP280Manager::_validateReading(float temp, float press, float alt) {
    if (temp < TEMP_MIN || temp > TEMP_MAX) return false;
    if (press < PRESSURE_MIN || press > PRESSURE_MAX) return false;
    if (_isFrozen(press)) return false;
    if ((millis() - _warmupStartTime) < WARMUP_DURATION) return true;
    
    if (_lastUpdateTime > 0) {
        float dt = (millis() - _lastUpdateTime) / 1000.0f;
        if (dt > 0.1f && dt < 10.0f) {
            if (!_checkRateOfChange(temp, press, alt, dt)) return false;
        }
    }
    
    if (_historyFull || _historyIndex >= 3) {
        if (_isOutlier(press, _pressureHistory, _historyFull ? HISTORY_SIZE : _historyIndex)) return false;
    }
    
    return true;
}

bool BMP280Manager::_isFrozen(float currentPressure) {
    if (fabs(currentPressure - _lastPressureRead) < 0.05f) { 
        _identicalReadings++;
    } else {
        _identicalReadings = 0;
    }
    _lastPressureRead = currentPressure;
    return (_identicalReadings >= 500); 
}

bool BMP280Manager::_checkRateOfChange(float temp, float press, float alt, float deltaTime) {
    uint8_t prevIdx = (_historyIndex == 0) ? HISTORY_SIZE - 1 : _historyIndex - 1;
    float pressRate = fabs(press - _pressureHistory[prevIdx]) / deltaTime;
    if (pressRate > (MAX_PRESSURE_RATE * 2.0f)) return false; 
    return true;
}

bool BMP280Manager::_isOutlier(float value, float* history, uint8_t count) const {
    if (count < 3) return false;
    
    float median = _getMedian(history, count);
    float deviations[HISTORY_SIZE];
    for (uint8_t i = 0; i < count; i++) deviations[i] = fabs(history[i] - median);
    
    float mad = _getMedian(deviations, count);
    if (mad < 0.1f) mad = 0.1f; 
    
    float score = fabs(value - median) / mad;
    return (score > 8.0f);
}

float BMP280Manager::_getMedian(float* values, uint8_t count) const {
    float sorted[HISTORY_SIZE];
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

void BMP280Manager::_updateHistory(float temp, float press, float alt) {
    _pressureHistory[_historyIndex] = press;
    _altitudeHistory[_historyIndex] = alt;
    _tempHistory[_historyIndex] = temp;
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;
    if (_historyIndex == 0) _historyFull = true;
    _lastUpdateTime = millis();
}

bool BMP280Manager::_canReinit() const {
    return ((millis() - _lastReinitTime) > REINIT_COOLDOWN);
}