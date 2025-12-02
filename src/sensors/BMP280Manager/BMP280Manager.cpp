/**
 * @file BMP280Manager.cpp
 * @brief Implementação limpa e otimizada do gerenciador BMP280
 */

#include "BMP280Manager.h"

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
    
    // 1. Tentar endereços I2C (0x76 e 0x77)
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
        DEBUG_PRINTLN("[BMP280Manager] ERRO: Sensor não encontrado.");
        return false;
    }
    
    // 2. Configuração Otimizada (Datasheet Handheld Device Dynamic)
    if (!_bmp280.configure(
        BMP280::Mode::NORMAL,
        BMP280::TempOversampling::X1,
        BMP280::PressOversampling::X8,  // Alta resolução
        BMP280::Filter::OFF,            // Filtro OFF para resposta rápida
        BMP280::StandbyTime::MS_125
    )) {
        DEBUG_PRINTLN("[BMP280Manager] ERRO: Falha na configuração.");
        return false;
    }
    
    // 3. Warm-up e teste inicial
    delay(100); 
    float t, p, a;
    if (_readRaw(t, p, a)) {
        _online = true;
        _tempValid = true;
        _warmupStartTime = millis();
        DEBUG_PRINTLN("[BMP280Manager] Inicializado com sucesso.");
        return true;
    }

    DEBUG_PRINTLN("[BMP280Manager] Aviso: Sensor detectado mas leitura falhou.");
    return false;
}

void BMP280Manager::update() {
    if (!_online) return;
    
    float temp, press, alt;
    
    // Tenta ler até 3 vezes em caso de falha I2C momentânea
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
            DEBUG_PRINTLN("[BMP280Manager] Falhas excessivas. Tentando reinicializar...");
            forceReinit();
        }
        return;
    }
    
    // Validação Lógica (Ranges, Travamento, Taxa de variação)
    if (!_validateReading(temp, press, alt)) {
        _failCount++;
        // Se falhar validação muitas vezes ou travar, força reinit
        if ((_failCount >= 20 || _identicalReadings >= MAX_IDENTICAL_READINGS) && _canReinit()) {
            DEBUG_PRINTLN("[BMP280Manager] Dados inválidos ou sensor travado. Resetando...");
            forceReinit();
        }
        return;
    }
    
    // Sucesso
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

void BMP280Manager::printStatus() const {
    if (_online) {
        DEBUG_PRINTF(" BMP280: ONLINE (T=%.2f C, P=%.1f hPa, A=%.0f m)\n", _temperature, _pressure, _altitude);
    } else {
        DEBUG_PRINTLN(" BMP280: OFFLINE");
    }
}

// ========================================
// MÉTODOS PRIVADOS - LEITURA & CÁLCULO
// ========================================

bool BMP280Manager::_readRaw(float& temp, float& press, float& alt) {
    temp = _bmp280.readTemperature();
    press = _bmp280.readPressure() / 100.0f; // Pa -> hPa
    alt = _bmp280.readAltitude();
    
    return (!isnan(temp) && !isnan(press) && !isnan(alt));
}

bool BMP280Manager::_validateReading(float temp, float press, float alt) {
    // 1. Range Físico (Datasheet)
    if (temp < TEMP_MIN || temp > TEMP_MAX) return false;
    if (press < PRESSURE_MIN || press > PRESSURE_MAX) return false;
    
    // 2. Detecção de Travamento (Valores idênticos repetidos)
    if (_isFrozen(press)) return false;
    
    // 3. Ignorar validação fina durante warm-up
    if ((millis() - _warmupStartTime) < WARMUP_DURATION) return true;
    
    // 4. Taxa de Variação e Outliers (apenas se tiver histórico)
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
    // Tolerância mínima para ruído do sensor
    if (fabs(currentPressure - _lastPressureRead) < 0.02f) { 
        _identicalReadings++;
    } else {
        _identicalReadings = 0;
    }
    _lastPressureRead = currentPressure;
    
    return (_identicalReadings >= MAX_IDENTICAL_READINGS);
}

bool BMP280Manager::_checkRateOfChange(float temp, float press, float alt, float deltaTime) {
    uint8_t prevIdx = (_historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE;
    
    float pressRate = fabs(press - _pressureHistory[prevIdx]) / deltaTime;
    float altRate = fabs(alt - _altitudeHistory[prevIdx]) / deltaTime;
    
    if (pressRate > MAX_PRESSURE_RATE) return false;
    if (altRate > MAX_ALTITUDE_RATE) return false;
    
    return true;
}

bool BMP280Manager::_isOutlier(float value, float* history, uint8_t count) const {
    if (count < 3) return false;
    float median = _getMedian(history, count);
    float deviations[HISTORY_SIZE];
    
    for (uint8_t i = 0; i < count; i++) deviations[i] = fabs(history[i] - median);
    
    float mad = _getMedian(deviations, count);
    if (mad < 0.1f) mad = 0.1f; // Evitar divisão por zero/ruído zero
    
    float score = fabs(value - median) / mad;
    return (score > 4.0f); // Tolerância (Z-Score modificado)
}

float BMP280Manager::_getMedian(float* values, uint8_t count) const {
    float sorted[HISTORY_SIZE];
    memcpy(sorted, values, count * sizeof(float));
    
    // Bubble sort simples para poucos elementos
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