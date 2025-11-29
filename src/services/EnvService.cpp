#include "EnvService.h"
#include <string.h>
#include <math.h>

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
      _lastSiRead(0),
      _bmpAddress(BMP280_ADDR_1),        // ✅ NOVO
      _bmpMode(BMP280_MODE_NORMAL)       // ✅ NOVO
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

// ====================== BMP280 - MÉTODOS AUXILIARES ======================

bool EnvService::_forceBMP280Measurement() {
    if (!_i2c) return false;
    
    // Ler configuração atual do registro de controle
    uint8_t ctrlReg = _i2c->readRegister(_bmpAddress, BMP280_REGISTER_CONTROL);
    
    // Forçar modo FORCED (bits [1:0] = 01 ou 10)
    // Mantém oversampling, apenas muda modo para FORCED
    ctrlReg = (ctrlReg & 0xFC) | BMP280_MODE_FORCED;
    
    // Escrever de volta
    _i2c->writeRegister(_bmpAddress, BMP280_REGISTER_CONTROL, ctrlReg);
    
    return true;
}

bool EnvService::_isBMP280Ready() {
    if (!_i2c) return false;
    
    uint8_t status = _i2c->readRegister(_bmpAddress, BMP280_REGISTER_STATUS);
    
    // Sensor está pronto quando:
    // - Bit 3 (measuring) = 0
    // - Bit 0 (im_update) = 0
    return ((status & (BMP280_STATUS_MEASURING | BMP280_STATUS_IM_UPDATE)) == 0);
}

bool EnvService::_waitForBMP280Measurement() {
    if (!_i2c) return false;
    
    // Timeout baseado no modo de oversampling:
    // - x1: ~10ms
    // - x2: ~15ms
    // - x4: ~25ms
    // - x8: ~45ms
    // - x16: ~80ms
    // - x16 temp + x16 press: ~115ms
    // Usamos 200ms para margem de segurança
    
    uint16_t maxRetries = 200;  // 200ms timeout
    uint16_t delayMs = 1;
    
    while (maxRetries--) {
        if (_isBMP280Ready()) {
            return true;
        }
        delay(delayMs);
    }
    
    DEBUG_PRINTLN("[EnvService] BMP280: Timeout aguardando medição");
    return false;
}

// ====================== BMP280 - INICIALIZAÇÃO ======================

bool EnvService::_initBMP280() {
    DEBUG_PRINTLN("[EnvService] ========================================");
    DEBUG_PRINTLN("[EnvService] Inicializando BMP280 (modo robusto v2)");
    DEBUG_PRINTLN("[EnvService] ========================================");

    // Reset estado
    _bmpOnline         = false;
    _bmpTempValid      = false;
    _warmupStartTime   = 0;
    _identicalReadings = 0;
    _lastPressureRead  = 0.0f;
    _bmpFailCount      = 0;

    delay(100);  // Aguardar estabilização inicial

    // ===== FASE 1: DETECÇÃO =====
    uint8_t addresses[] = {BMP280_ADDR_1, BMP280_ADDR_2};
    bool found = false;

    for (uint8_t addr : addresses) {
        DEBUG_PRINTF("[EnvService] Tentando BMP280 em 0x%02X...\n", addr);

        for (int attempt = 0; attempt < 5; attempt++) {
            if (_bmp.begin(addr)) {
                found = true;
                _bmpAddress = addr;
                DEBUG_PRINTF("[EnvService] ✓ BMP280 detectado em 0x%02X (tentativa %d)\n",
                             addr, attempt + 1);
                break;
            }
            delay(200);
        }
        if (found) break;
    }

    if (!found) {
        DEBUG_PRINTLN("[EnvService] ✗ BMP280 não detectado no barramento I2C");
        return false;
    }

    // ===== FASE 2: CONFIGURAÇÃO =====
    DEBUG_PRINTLN("[EnvService] Aplicando configuração...");
    
    // Configuração já foi aplicada pelo _bmp.begin()
    // Aqui podemos forçar modo específico se necessário
    _bmpMode = BMP280_MODE_NORMAL;  // ou FORCED para economia de energia
    
    DEBUG_PRINTF("[EnvService]   Modo: %s\n", 
                 _bmpMode == BMP280_MODE_NORMAL ? "NORMAL" : "FORCED");
    DEBUG_PRINTLN("[EnvService]   Oversampling: conforme BMP280Hal");

    // ===== FASE 3: WARMUP =====
    DEBUG_PRINTLN("[EnvService] Aguardando warm-up (3 segundos)...");
    delay(3000);

    // ===== FASE 4: TESTE DE LEITURAS =====
    DEBUG_PRINTLN("[EnvService] Testando leituras (5 tentativas)...");
    
    int successfulReads = 0;
    const int requiredSuccess = 3;  // Aceitar 3/5 leituras válidas
    const int totalAttempts = 5;
    
    float lastValidTemp = NAN;
    float lastValidPress = NAN;

    for (int i = 0; i < totalAttempts; i++) {
        bool readSuccess = false;
        float temp = NAN;
        float press = NAN;
        
        // Se modo FORCED, iniciar medição
        if (_bmpMode == BMP280_MODE_FORCED) {
            _forceBMP280Measurement();
        }
        
        // Aguardar medição completar
        delay(120);  // Tempo seguro para oversample máximo
        
        // Tentar ler até 3 vezes
        for (int retry = 0; retry < 3; retry++) {
            // IMPORTANTE: Sempre ler temperatura ANTES de pressão
            temp = _bmp.readTemperature();
            
            if (!isnan(temp)) {
                press = _bmp.readPressure() / 100.0f;  // Pa → hPa
            }
            
            // Validação básica
            bool tempValid = (!isnan(temp) && temp > -40.0f && temp < 85.0f);
            bool pressValid = (!isnan(press) && press > 300.0f && press < 1200.0f);
            
            if (tempValid && pressValid) {
                readSuccess = true;
                break;
            }
            
            DEBUG_PRINTF("[EnvService]     Tentativa %d.%d: T=%.1f P=%.0f [%s]\n",
                         i + 1, retry + 1, temp, press,
                         (!tempValid ? "T inválida" : "P inválida"));
            
            delay(50);
        }
        
        if (readSuccess) {
            successfulReads++;
            lastValidTemp = temp;
            lastValidPress = press;
            
            DEBUG_PRINTF("[EnvService]   ✓ Leitura %d/%d: T=%.1f°C P=%.0f hPa\n",
                         i + 1, totalAttempts, temp, press);
            
            // Inicializar histórico na primeira leitura válida
            if (successfulReads == 1) {
                float alt = _calculateAltitude(press);
                for (int j = 0; j < 5; j++) {
                    _pressureHistory[j] = press;
                    _altitudeHistory[j] = alt;
                    _tempHistory[j]     = temp;
                }
                
                _temperatureBMP = temp;
                _pressure = press;
                _altitude = alt;
            }
        } else {
            DEBUG_PRINTF("[EnvService]   ✗ Leitura %d/%d: FALHA\n", i + 1, totalAttempts);
        }
        
        delay(200);
    }

    // ===== FASE 5: DECISÃO FINAL =====
    if (successfulReads >= requiredSuccess) {
        _bmpOnline    = true;
        _bmpTempValid = true;
        _bmpFailCount = 0;
        _historyFull  = true;
        _warmupStartTime = millis();

        DEBUG_PRINTLN("[EnvService] ========================================");
        DEBUG_PRINTF("[EnvService] ✓ BMP280 INICIALIZADO COM SUCESSO!\n");
        DEBUG_PRINTF("[EnvService]   Leituras válidas: %d/%d\n", 
                     successfulReads, totalAttempts);
        DEBUG_PRINTF("[EnvService]   Última leitura: T=%.1f°C P=%.0f hPa\n",
                     lastValidTemp, lastValidPress);
        DEBUG_PRINTLN("[EnvService] ========================================");

        return true;
    }

    DEBUG_PRINTLN("[EnvService] ========================================");
    DEBUG_PRINTF("[EnvService] ✗ BMP280 REJEITADO\n");
    DEBUG_PRINTF("[EnvService]   Leituras válidas: %d/%d (mínimo: %d)\n",
                 successfulReads, totalAttempts, requiredSuccess);
    DEBUG_PRINTLN("[EnvService] ========================================");
    
    return false;
}

// ====================== BMP280 - UPDATE ======================

void EnvService::_updateBMP280() {
    if (!_bmpOnline) {
        _temperatureBMP = NAN;
        return;
    }

    bool readSuccess = false;
    float temp = NAN, press = NAN, alt = NAN;
    
    // Se modo FORCED, iniciar nova medição
    if (_bmpMode == BMP280_MODE_FORCED) {
        _forceBMP280Measurement();
    }

    // Tentar ler até 3 vezes
    for (int retry = 0; retry < 3; retry++) {
        // Aguardar medição se necessário
        if (retry == 0) {
            delay(50);  // Primeira tentativa: delay curto
        } else {
            delay(100);  // Retries: delay maior
        }
        
        if (!_waitForBMP280Measurement()) {
            DEBUG_PRINTF("[EnvService] BMP280: Timeout no retry %d\n", retry + 1);
            continue;
        }

        // SEMPRE ler temperatura antes de pressão
        temp = _bmp.readTemperature();
        
        if (!isnan(temp)) {
            press = _bmp.readPressure() / 100.0f;
            alt = _calculateAltitude(press);
        }

        // Validação
        if (!isnan(temp) && !isnan(press) && !isnan(alt) &&
            temp > -40.0f && temp < 85.0f &&
            press > 300.0f && press < 1200.0f) {
            readSuccess = true;
            break;
        }

        if (retry < 2) {
            DEBUG_PRINTF("[EnvService] BMP280: Retry %d (valores inválidos)\n", retry + 1);
        }
    }

    if (!readSuccess) {
        _bmpFailCount++;
        
        if (_bmpFailCount >= 3) {
            DEBUG_PRINTF("[EnvService] BMP280: %d falhas consecutivas\n", _bmpFailCount);
        }
        
        // Após 10 falhas, marcar temperatura como inválida
        if (_bmpFailCount >= 10) {
            _bmpTempValid = false;
            DEBUG_PRINTLN("[EnvService] BMP280: Marcado como instável");
        }
        
        return;
    }

    // Backup valores anteriores
    float tempBackup  = _temperatureBMP;
    float pressBackup = _pressure;
    float altBackup   = _altitude;

    _temperatureBMP = temp;
    _pressure       = press;
    _altitude       = alt;

    // Validação avançada
    if (!_validateBMP280Reading()) {
        // Restaurar valores anteriores
        _temperatureBMP = tempBackup;
        _pressure       = pressBackup;
        _altitude       = altBackup;

        _bmpFailCount++;
        
        DEBUG_PRINTF("[EnvService] BMP280: Leitura rejeitada (validação avançada)\n");
        return;
    }

    // Sucesso: resetar contadores
    _bmpTempValid    = true;
    _bmpFailCount    = 0;
    _bmpTempFailures = 0;
}

// ====================== MÉTODOS AUXILIARES (mantidos do original) ======================

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

    // Detecção de sensor travado
    bool exactlyIdentical = (memcmp(&press, &_lastPressureRead, sizeof(float)) == 0);

    if (exactlyIdentical && _lastPressureRead != 0.0f) {
        _identicalReadings++;

        if (_identicalReadings >= 20) {  // Reduzido de 50 para 20
            DEBUG_PRINTF("[EnvService] BMP280: TRAVADO! (P=%.2f por %d leituras)\n",
                         press, _identicalReadings);
            _identicalReadings = 0;
            return false;
        }
    } else {
        _identicalReadings = 0;
    }
    _lastPressureRead = press;

    // Validação de taxa de mudança
    unsigned long now = millis();
    float deltaTime = (now - _lastUpdateTime) / 1000.0f;

    if (_lastUpdateTime > 0 && deltaTime > 0.1f && deltaTime < 10.0f) {
        float pressRate = fabsf(press - _pressureHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (pressRate > 15.0f) {  // Reduzido de 20 para 15
            DEBUG_PRINTF("[EnvService] BMP280: Taxa pressão anormal: %.1f hPa/s\n", pressRate);
            return false;
        }

        float altRate = fabsf(alt - _altitudeHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (altRate > 100.0f) {  // Reduzido de 150 para 100
            DEBUG_PRINTF("[EnvService] BMP280: Taxa altitude anormal: %.1f m/s\n", altRate);
            return false;
        }

        float tempRate = fabsf(temp - _tempHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (tempRate > 0.5f) {  // Aumentado de 0.1 para 0.5 (mais realista)
            DEBUG_PRINTF("[EnvService] BMP280: Taxa temp anormal: %.2f°C/s\n", tempRate);
            return false;
        }
    }

    // Warm-up period
    if (_warmupStartTime == 0) {
        _warmupStartTime = millis();
    }

    unsigned long warmupElapsed = millis() - _warmupStartTime;

    // Validação de outliers (após warm-up)
    if (warmupElapsed >= 30000) {  // Após 30s
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

// ====================== SI7021 (mantido do original) ======================

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

// ====================== COMUM ======================

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
    // Prioridade: SI7021 > BMP280
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
