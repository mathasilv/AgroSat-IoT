/**
 * @file BMP280Manager.cpp
 * @brief Implementação do gerenciador BMP280
 */

#include "BMP280Manager.h"

// ============================================================================
// CONSTRUTOR
// ============================================================================
BMP280Manager::BMP280Manager() 
    : _bmp280(Wire),
      _temperature(NAN),
      _pressure(NAN),
      _altitude(NAN),
      _online(false),
      _tempValid(false),
      _failCount(0),
      _tempFailures(0),
      _historyIndex(0),
      _historyFull(false),
      _lastReinitTime(0),
      _warmupStartTime(0),
      _lastPressureRead(0.0f),
      _identicalReadings(0),
      _lastUpdateTime(0)
{
    // Inicializar histórico com valores padrão
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        _pressureHistory[i] = 1013.25f;
        _altitudeHistory[i] = 0.0f;
        _tempHistory[i] = 20.0f;
    }
}

// ============================================================================
// INICIALIZAÇÃO
// ============================================================================
bool BMP280Manager::begin() {
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    DEBUG_PRINTLN("[BMP280Manager] Inicializando sensor BMP280");
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    
    _online = false;
    _tempValid = false;
    _warmupStartTime = 0;
    _identicalReadings = 0;
    _lastPressureRead = 0.0f;
    _failCount = 0;
    
    // Soft reset antes de inicializar
    if (!_softReset()) {
        DEBUG_PRINTLN("[BMP280Manager] Soft reset falhou (sensor pode não responder)");
    }
    
    delay(200);
    
    // Tentar ambos os endereços possíveis
    uint8_t addresses[] = {BMP280::I2C_ADDR_PRIMARY, BMP280::I2C_ADDR_SECONDARY};
    bool found = false;
    
    for (uint8_t addr : addresses) {
        DEBUG_PRINTF("[BMP280Manager] Tentando endereço 0x%02X...\n", addr);
        
        for (int attempt = 0; attempt < 5; attempt++) {
            if (_bmp280.begin(addr)) {
                found = true;
                DEBUG_PRINTF("[BMP280Manager] Sensor detectado em 0x%02X (tentativa %d)\n", 
                            addr, attempt + 1);
                break;
            }
            delay(200);
        }
        
        if (found) break;
    }
    
    if (!found) {
        DEBUG_PRINTLN("[BMP280Manager] Sensor não detectado");
        return false;
    }
    
    // Configurar modo ótimo (já feito no BMP280::begin, mas explícito)
    if (!_bmp280.configure(
        BMP280::Mode::NORMAL,
        BMP280::TempOversampling::X2,
        BMP280::PressOversampling::X16,
        BMP280::Filter::X16,
        BMP280::StandbyTime::MS_500
    )) {
        DEBUG_PRINTLN("[BMP280Manager] Falha ao configurar sensor");
        return false;
    }
    
    DEBUG_PRINTLN("[BMP280Manager] Configuração aplicada");
    
    // Aguardar estabilização (5 segundos)
    DEBUG_PRINTLN("[BMP280Manager] Aguardando estabilização (5s)...");
    delay(5000);
    
    // Testar leituras iniciais (5 ciclos)
    DEBUG_PRINTLN("[BMP280Manager] Testando leituras...");
    float temp, press, alt;
    
    for (int i = 0; i < 5; i++) {
        if (!_readRaw(temp, press, alt)) {
            DEBUG_PRINTF("[BMP280Manager] Falha na leitura %d\n", i+1);
            return false;
        }
        
        DEBUG_PRINTF("[BMP280Manager] Leitura %d: T=%.1f°C P=%.0f hPa A=%.0f m\n",
                    i+1, temp, press, alt);
        
        // Inicializar histórico na última iteração
        if (i == 4) {
            _initHistory(temp, press, alt);
        }
        
        delay(200);
    }
    
    _online = true;
    _tempValid = true;
    _failCount = 0;
    _warmupStartTime = millis();
    
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    DEBUG_PRINTLN("[BMP280Manager] INICIALIZADO COM SUCESSO!");
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    
    return true;
}

// ============================================================================
// ATUALIZAÇÃO PRINCIPAL
// ============================================================================
void BMP280Manager::update() {
    if (!_online) {
        _temperature = NAN;
        _pressure = NAN;
        _altitude = NAN;
        return;
    }
    
    float temp, press, alt;
    
    // Tentar ler com retry (até 3 tentativas)
    bool readSuccess = false;
    for (int retry = 0; retry < 3; retry++) {
        if (_readRaw(temp, press, alt)) {
            readSuccess = true;
            break;
        }
        
        if (retry < 2) {
            DEBUG_PRINTF("[BMP280Manager] Retry %d\n", retry + 1);
            delay(50);
        }
    }
    
    if (!readSuccess) {
        _failCount++;
        DEBUG_PRINTLN("[BMP280Manager] Falha após 3 tentativas");
        
        // Reinicializar após 5 falhas
        if (_needsReinit() && _canReinit()) {
            DEBUG_PRINTLN("[BMP280Manager] 5 falhas detectadas, reinicializando...");
            forceReinit();
        }
        return;
    }
    
    // Validar leitura
    if (!_validateReading(temp, press, alt)) {
        _failCount++;
        _tempFailures++;
        DEBUG_PRINTF("[BMP280Manager] Leitura inválida (P=%.0f hPa)\n", press);
        
        // Se detectou anomalia grande ou sensor travado, forçar reinit imediato
        bool bigAnomaly = abs(press - _pressure) > 50.0f;
        bool frozen = _isFrozen(press);
        
        if ((bigAnomaly || frozen) && _canReinit()) {
            if (frozen) {
                DEBUG_PRINTLN("[BMP280Manager] SENSOR TRAVADO! Reinicializando...");
            } else {
                DEBUG_PRINTLN("[BMP280Manager] Anomalia grande detectada, reinicializando...");
            }
            forceReinit();
        }
        return;
    }
    
    // Leitura válida - atualizar valores
    _temperature = temp;
    _pressure = press;
    _altitude = alt;
    _tempValid = true;
    _failCount = 0;
    _tempFailures = 0;
    
    // Atualizar histórico
    _updateHistory(temp, press, alt);
}

// ============================================================================
// REINICIALIZAÇÃO FORÇADA
// ============================================================================
void BMP280Manager::forceReinit() {
    DEBUG_PRINTLN("[BMP280Manager] Reinicialização forçada...");
    _lastReinitTime = millis();
    _online = begin();
}

void BMP280Manager::reset() {
    _bmp280.reset();
    _online = false;
    _tempValid = false;
    _failCount = 0;
    delay(100);
}

// ============================================================================
// DIAGNÓSTICO
// ============================================================================
void BMP280Manager::printStatus() const {
    DEBUG_PRINTF(" BMP280: %s", _online ? "ONLINE" : "OFFLINE");
    if (_online) {
        DEBUG_PRINTF(" (Temp: %s, Falhas: %d)", 
                    _tempValid ? "OK" : "FALHA", 
                    _failCount);
    }
    DEBUG_PRINTLN();
    
    if (_online) {
        DEBUG_PRINTF("   T=%.2f°C P=%.1f hPa A=%.0f m\n", 
                    _temperature, _pressure, _altitude);
    }
}

// ============================================================================
// MÉTODOS INTERNOS - LEITURA
// ============================================================================
bool BMP280Manager::_readRaw(float& temp, float& press, float& alt) {
    temp = _bmp280.readTemperature();
    press = _bmp280.readPressure() / 100.0f; // Converter Pa para hPa
    alt = _bmp280.readAltitude();
    
    // Verificar valores NaN
    if (isnan(temp) || isnan(press) || isnan(alt)) {
        return false;
    }
    
    return true;
}

// ============================================================================
// MÉTODOS INTERNOS - VALIDAÇÃO
// ============================================================================
bool BMP280Manager::_validateReading(float temp, float press, float alt) {
    // Validação básica de range
    if (temp < TEMP_MIN || temp > TEMP_MAX) {
        DEBUG_PRINTF("[BMP280Manager] Temperatura fora do range: %.1f°C\n", temp);
        return false;
    }
    
    if (press < PRESSURE_MIN || press > PRESSURE_MAX) {
        DEBUG_PRINTF("[BMP280Manager] Pressão fora do range: %.0f hPa\n", press);
        return false;
    }
    
    // Detecção de travamento
    if (_isFrozen(press)) {
        return false;
    }
    
    // Durante warm-up, não aplicar validações avançadas
    unsigned long warmupElapsed = millis() - _warmupStartTime;
    if (warmupElapsed < WARMUP_DURATION) {
        DEBUG_PRINTF("[BMP280Manager] Warm-up (%lus/30s)\n", warmupElapsed / 1000);
        return true;
    }
    
    // Após warm-up, validar taxa de mudança
    if (_lastUpdateTime > 0) {
        float deltaTime = (millis() - _lastUpdateTime) / 1000.0f;
        if (deltaTime > 0.1f && deltaTime < 10.0f) {
            if (!_checkRateOfChange(temp, press, alt, deltaTime)) {
                return false;
            }
        }
    }
    
    // Detecção de outliers (se histórico suficiente)
    if (_historyFull || _historyIndex >= 3) {
        uint8_t count = _historyFull ? HISTORY_SIZE : _historyIndex;
        if (_isOutlier(press, _pressureHistory, count)) {
            DEBUG_PRINTF("[BMP280Manager] Pressão outlier: %.0f hPa\n", press);
            return false;
        }
    }
    
    return true;
}

bool BMP280Manager::_checkRateOfChange(float temp, float press, float alt, float deltaTime) {
    uint8_t prevIdx = (_historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE;
    
    // Taxa de mudança de pressão
    float pressRate = abs(press - _pressureHistory[prevIdx]) / deltaTime;
    if (pressRate > MAX_PRESSURE_RATE) {
        DEBUG_PRINTF("[BMP280Manager] Taxa pressão anormal: %.1f hPa/s\n", pressRate);
        return false;
    }
    
    // Taxa de mudança de altitude
    float altRate = abs(alt - _altitudeHistory[prevIdx]) / deltaTime;
    if (altRate > MAX_ALTITUDE_RATE) {
        DEBUG_PRINTF("[BMP280Manager] Taxa altitude anormal: %.1f m/s\n", altRate);
        return false;
    }
    
    // Taxa de mudança de temperatura
    float tempRate = abs(temp - _tempHistory[prevIdx]) / deltaTime;
    if (tempRate > MAX_TEMP_RATE) {
        DEBUG_PRINTF("[BMP280Manager] Taxa temp anormal: %.2f°C/s\n", tempRate);
        return false;
    }
    
    return true;
}

bool BMP280Manager::_isFrozen(float currentPressure) {
    // Comparação bitwise exata (não aproximada)
    bool exactlyIdentical = (memcmp(&currentPressure, &_lastPressureRead, sizeof(float)) == 0);
    
    if (exactlyIdentical && _lastPressureRead != 0.0f) {
        _identicalReadings++;
        
        if (_identicalReadings >= MAX_IDENTICAL_READINGS) {
            DEBUG_PRINTF("[BMP280Manager] SENSOR TRAVADO! (%d leituras idênticas)\n", 
                        _identicalReadings);
            _identicalReadings = 0;
            return true;
        }
    } else {
        _identicalReadings = 0;
    }
    
    _lastPressureRead = currentPressure;
    return false;
}

bool BMP280Manager::_isOutlier(float value, float* history, uint8_t count) const {
    if (count < 3) return false;
    
    float median = _getMedian(history, count);
    float deviations[HISTORY_SIZE];
    
    for (uint8_t i = 0; i < count; i++) {
        deviations[i] = abs(history[i] - median);
    }
    
    float mad = _getMedian(deviations, count);
    if (mad < 0.1f) mad = 0.1f;
    
    float score = abs(value - median) / mad;
    return (score > 3.0f); // Outlier se > 3 MAD
}

float BMP280Manager::_getMedian(float* values, uint8_t count) const {
    float sorted[HISTORY_SIZE];
    memcpy(sorted, values, count * sizeof(float));
    
    // Bubble sort
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

// ============================================================================
// MÉTODOS INTERNOS - HISTÓRICO
// ============================================================================
void BMP280Manager::_updateHistory(float temp, float press, float alt) {
    _pressureHistory[_historyIndex] = press;
    _altitudeHistory[_historyIndex] = alt;
    _tempHistory[_historyIndex] = temp;
    
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;
    if (_historyIndex == 0) {
        _historyFull = true;
    }
    
    _lastUpdateTime = millis();
}

void BMP280Manager::_initHistory(float temp, float press, float alt) {
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        _pressureHistory[i] = press;
        _altitudeHistory[i] = alt;
        _tempHistory[i] = temp;
    }
    _historyFull = true;
    _historyIndex = 0;
}

// ============================================================================
// MÉTODOS INTERNOS - REINICIALIZAÇÃO
// ============================================================================
bool BMP280Manager::_needsReinit() const {
    return (_failCount >= 5);
}

bool BMP280Manager::_canReinit() const {
    return ((millis() - _lastReinitTime) > REINIT_COOLDOWN);
}

bool BMP280Manager::_softReset() {
    // O BMP280::reset() já faz soft reset interno
    _bmp280.reset();
    delay(50);
    return true;
}

bool BMP280Manager::_waitForMeasurement() {
    // Não necessário - o BMP280 em modo NORMAL atualiza automaticamente
    return true;
}

float BMP280Manager::_calculateAltitude(float pressure) const {
    // Fórmula barométrica padrão
    return 44330.0f * (1.0f - pow(pressure / 1013.25f, 0.1903f));
}
