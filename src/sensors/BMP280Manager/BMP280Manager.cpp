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

bool BMP280Manager::begin() {
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    DEBUG_PRINTLN("[BMP280Manager] Inicializando BMP280 Pressure Sensor");
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    
    _online = false;
    _tempValid = false;
    _warmupStartTime = 0;
    _identicalReadings = 0;
    _lastPressureRead = 0.0f;
    _failCount = 0;
    _historyIndex = 0;
    _historyFull = false;
    _lastUpdateTime = 0;
    
    // Inicializar histórico com valores padrão
    for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        _pressureHistory[i] = 1013.25f;  // Pressão ao nível do mar
        _altitudeHistory[i] = 0.0f;
        _tempHistory[i] = 20.0f;
    }
    
    // PASSO 1: Soft reset
    DEBUG_PRINTLN("[BMP280Manager] PASSO 1: Soft reset...");
    if (!_softReset()) {
        DEBUG_PRINTLN("[BMP280Manager] Soft reset falhou (continuando)");
    } else {
        DEBUG_PRINTLN("[BMP280Manager] Soft reset OK");
    }
    delay(200);  // Aguardar sensor reiniciar
    
    // PASSO 2: Tentar endereços I2C
    DEBUG_PRINTLN("[BMP280Manager] PASSO 2: Detectando sensor...");
    uint8_t addresses[] = {BMP280::I2C_ADDR_PRIMARY, BMP280::I2C_ADDR_SECONDARY};
    bool found = false;
    
    for (uint8_t addr : addresses) {
        DEBUG_PRINTF("[BMP280Manager]   Testando 0x%02X... ", addr);
        
        if (_bmp280.begin(addr)) {
            found = true;
            DEBUG_PRINTF("✓ DETECTADO!\n");
            break;
        }
        DEBUG_PRINTLN("✗");
        delay(100);
    }
    
    if (!found) {
        DEBUG_PRINTLN("[BMP280Manager] FALHA: Sensor não detectado");
        return false;
    }
    
    // PASSO 3: Configurar sensor (baseado no datasheet)
    DEBUG_PRINTLN("[BMP280Manager] PASSO 3: Configurando sensor...");
    DEBUG_PRINTLN("[BMP280Manager]   Mode: NORMAL (medição contínua)");
    DEBUG_PRINTLN("[BMP280Manager]   Temp Oversampling: x1");
    DEBUG_PRINTLN("[BMP280Manager]   Press Oversampling: x8 (alta resolução)");
    DEBUG_PRINTLN("[BMP280Manager]   Filter: OFF");
    DEBUG_PRINTLN("[BMP280Manager]   Standby: 125ms");
    
    if (!_bmp280.configure(
        BMP280::Mode::NORMAL,
        BMP280::TempOversampling::X1,
        BMP280::PressOversampling::X8,  // Alta resolução
        BMP280::Filter::OFF,
        BMP280::StandbyTime::MS_125
    )) {
        DEBUG_PRINTLN("[BMP280Manager] FALHA: Configuração falhou");
        return false;
    }
    
    DEBUG_PRINTLN("[BMP280Manager] Configuração OK");
    
    // PASSO 4: Warm-up (2 segundos)
    DEBUG_PRINTLN("[BMP280Manager] PASSO 4: Warm-up (2 segundos)...");
    uint32_t start = millis();
    while (millis() - start < 2000) {
        if (_readRawFast()) {
            DEBUG_PRINTLN("[BMP280Manager] Primeira leitura OK");
            break;
        }
        delay(100);
    }
    
    // PASSO 5: Teste de leituras (3 tentativas)
    DEBUG_PRINTLN("[BMP280Manager] PASSO 5: Teste de leituras...");
    float temp, press, alt;
    int okCount = 0;
    
    for (int i = 0; i < 3; i++) {
        if (_readRaw(temp, press, alt)) {
            DEBUG_PRINTF("[BMP280Manager]   Leitura %d: T=%.1f°C P=%.0f hPa A=%.0f m\n", 
                        i+1, temp, press, alt);
            okCount++;
        } else {
            DEBUG_PRINTF("[BMP280Manager]   Leitura %d: ✗ FALHOU\n", i+1);
        }
        delay(100);
    }
    
    if (okCount < 2) {
        DEBUG_PRINTLN("[BMP280Manager] FALHA: Poucas leituras válidas");
        return false;
    }
    
    // SUCESSO!
    _online = true;
    _tempValid = true;
    _failCount = 0;
    _warmupStartTime = millis();
    
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    DEBUG_PRINTLN("[BMP280Manager] BMP280 INICIALIZADO COM SUCESSO!");
    DEBUG_PRINTLN("[BMP280Manager] ========================================");
    DEBUG_PRINTLN("[BMP280Manager] OBSERVAÇÕES:");
    DEBUG_PRINTLN("[BMP280Manager] - Warm-up de 2s para estabilização");
    DEBUG_PRINTLN("[BMP280Manager] - Detecção automática de travamento");
    DEBUG_PRINTLN("[BMP280Manager] - Validação estatística ativa");
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
    
    // Tentar ler com retry (3 tentativas)
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
        
        // Detectar anomalia grande ou sensor travado
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
    press = _bmp280.readPressure() / 100.0f;
    alt = _bmp280.readAltitude();
    
    if (isnan(temp) || isnan(press) || isnan(alt)) {
        return false;
    }
    
    return true;
}

bool BMP280Manager::_readRawFast() {
    float temp, press, alt;
    return _readRaw(temp, press, alt);
}

// ============================================================================
// MÉTODOS INTERNOS - VALIDAÇÃO
// ============================================================================
bool BMP280Manager::_validateReading(float temp, float press, float alt) {
    // 1. Validar range de temperatura (datasheet: -40°C a +85°C)
    if (temp < TEMP_MIN || temp > TEMP_MAX) {
        DEBUG_PRINTF("[BMP280Manager] Temperatura fora do range: %.1f°C (válido: %.0f a %.0f)\n", 
                    temp, TEMP_MIN, TEMP_MAX);
        return false;
    }
    
    // 2. Validar range de pressão (datasheet: 300 hPa a 1100 hPa)
    if (press < PRESSURE_MIN || press > PRESSURE_MAX) {
        DEBUG_PRINTF("[BMP280Manager] Pressão fora do range: %.0f hPa (válido: %.0f a %.0f)\n", 
                    press, PRESSURE_MIN, PRESSURE_MAX);
        return false;
    }
    
    // 3. Detecção de travamento (leituras idênticas consecutivas)
    if (_isFrozen(press)) {
        DEBUG_PRINTLN("[BMP280Manager] Sensor travado detectado");
        return false;
    }
    
    // 4. Durante warm-up inicial, aceitar leituras sem validação adicional
    unsigned long warmupElapsed = millis() - _warmupStartTime;
    if (warmupElapsed < WARMUP_DURATION) {
        return true;
    }
    
    // 5. Após warm-up, validar taxa de mudança
    if (_lastUpdateTime > 0) {
        float deltaTime = (millis() - _lastUpdateTime) / 1000.0f;  // segundos
        
        // Validar apenas se deltaTime está em range razoável
        if (deltaTime > 0.1f && deltaTime < 10.0f) {
            if (!_checkRateOfChange(temp, press, alt, deltaTime)) {
                DEBUG_PRINTLN("[BMP280Manager] Taxa de mudança anormal");
                return false;
            }
        }
    }
    
    // 6. Detecção de outliers estatísticos (apenas se houver histórico suficiente)
    if (_historyFull || _historyIndex >= 3) {
        uint8_t count = _historyFull ? HISTORY_SIZE : _historyIndex;
        
        if (_isOutlier(press, _pressureHistory, count)) {
            DEBUG_PRINTF("[BMP280Manager] Pressão é outlier: %.0f hPa\n", press);
            return false;
        }
        
        if (_isOutlier(temp, _tempHistory, count)) {
            DEBUG_PRINTF("[BMP280Manager] Temperatura é outlier: %.1f°C\n", temp);
            return false;
        }
    }
    
    return true;
}

bool BMP280Manager::_checkRateOfChange(float temp, float press, float alt, float deltaTime) {
    // Obter índice da última leitura no histórico
    uint8_t prevIdx = (_historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE;
    
    // 1. Validar taxa de mudança de pressão
    float pressRate = fabs(press - _pressureHistory[prevIdx]) / deltaTime;
    if (pressRate > MAX_PRESSURE_RATE) {
        DEBUG_PRINTF("[BMP280Manager] Taxa de pressão anormal: %.1f hPa/s (max: %.1f)\n", 
                    pressRate, MAX_PRESSURE_RATE);
        return false;
    }
    
    // 2. Validar taxa de mudança de altitude
    float altRate = fabs(alt - _altitudeHistory[prevIdx]) / deltaTime;
    if (altRate > MAX_ALTITUDE_RATE) {
        DEBUG_PRINTF("[BMP280Manager] Taxa de altitude anormal: %.1f m/s (max: %.1f)\n", 
                    altRate, MAX_ALTITUDE_RATE);
        return false;
    }
    
    // 3. Validar taxa de mudança de temperatura
    float tempRate = fabs(temp - _tempHistory[prevIdx]) / deltaTime;
    if (tempRate > MAX_TEMP_RATE) {
        DEBUG_PRINTF("[BMP280Manager] Taxa de temperatura anormal: %.2f°C/s (max: %.1f)\n", 
                    tempRate, MAX_TEMP_RATE);
        return false;
    }
    
    return true;
}

// Na função _isFrozen() - linha ~320
bool BMP280Manager::_isFrozen(float currentPressure) {
    // Mudar de 50 → 200 leituras idênticas
    if (_identicalReadings >= MAX_IDENTICAL_READINGS) {  // ← AUMENTAR!
        DEBUG_PRINTF("[BMP280Manager] SENSOR TRAVADO! (%d leituras idênticas)\n", 
                    _identicalReadings);
        _identicalReadings = 0;
        return true;
    }
    
    float tolerance = 0.05f;  // 0.01 hPa (~ruído normal)
    bool withinTolerance = fabs(currentPressure - _lastPressureRead) < 0.05f;  
    
    if (withinTolerance && _lastPressureRead != 0.0f) {
        _identicalReadings++;
    } else {
        _identicalReadings = 0;
    }
    
    _lastPressureRead = currentPressure;
    return false;
}


bool BMP280Manager::_isOutlier(float value, float* history, uint8_t count) const {
    if (count < 3) return false;  // Mínimo de 3 amostras
    
    // 1. Calcular mediana do histórico
    float median = _getMedian(history, count);
    
    // 2. Calcular desvios absolutos
    float deviations[HISTORY_SIZE];
    for (uint8_t i = 0; i < count; i++) {
        deviations[i] = fabs(history[i] - median);
    }
    
    // 3. Calcular MAD (mediana dos desvios)
    float mad = _getMedian(deviations, count);
    
    // 4. Evitar divisão por zero (todos valores idênticos)
    if (mad < 0.1f) mad = 0.1f;
    
    // 5. Calcular score (quantos MADs o valor está da mediana)
    float score = fabs(value - median) / mad;
    
    // 6. Considerar outlier se score > 3.0 (critério estatístico padrão)
    return (score > 3.0f);
}

float BMP280Manager::_getMedian(float* values, uint8_t count) const {
    // Criar cópia para não modificar array original
    float sorted[HISTORY_SIZE];
    memcpy(sorted, values, count * sizeof(float));
    
    // Bubble sort (eficiente para arrays pequenos como HISTORY_SIZE=10)
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = 0; j < count - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                float temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    // Retornar elemento do meio
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
    _bmp280.reset();
    delay(50);
    return true;
}

bool BMP280Manager::_waitForMeasurement() {
    return true;
}

float BMP280Manager::_calculateAltitude(float pressure) const {
    return 44330.0f * (1.0f - pow(pressure / 1013.25f, 0.1903f));
}
