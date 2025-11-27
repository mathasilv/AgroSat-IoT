/**
 * @file SensorManager.cpp
 * @brief Gerenciamento de sensores PION - VERSÃO OTIMIZADA
 * @version 4.0.0
 * @date 2025-11-11
 * 
 * MUDANÇAS v4.0.0:
 * - CCS811 warmup 20s obrigatório
 * - SI7021 com polling adequado (15ms + retry)
 * - Magnetômetro calibrado
 * - Filtro otimizado (multiplicação vs divisão)
 * - Compensação ambiental CCS811
 */

#include "SensorManager.h"
#include <algorithm>
#include "DisplayManager.h"
#include <string.h> 

SensorManager::SensorManager(HAL::I2C& i2c) :
    _i2c(&i2c),
    _mpu9250(i2c, MPU9250_ADDRESS),
    _bmp280(i2c),
    _si7021(i2c),
    _ccs811(i2c),
    _temperature(NAN), _temperatureBMP(NAN), _temperatureSI(NAN), 
    _pressure(NAN), _altitude(NAN),
    _humidity(NAN), _co2Level(NAN), _tvoc(NAN),
    _seaLevelPressure(1013.25),
    _gyroX(0.0f), _gyroY(0.0f), _gyroZ(0.0f),
    _accelX(0.0f), _accelY(0.0f), _accelZ(0.0f),
    _magX(0.0f), _magY(0.0f), _magZ(0.0f),
    _magOffsetX(0.0f), _magOffsetY(0.0f), _magOffsetZ(0.0f),
    _mpu9250Online(false), _bmp280Online(false),
    _si7021Online(false), _ccs811Online(false),
    _calibrated(false),
    _si7021TempValid(false), _bmp280TempValid(false),
    _si7021TempFailures(0), _bmp280TempFailures(0),
    _lastReadTime(0), _lastCCS811Read(0), _lastSI7021Read(0),
    _lastHealthCheck(0), _consecutiveFailures(0), _filterIndex(0),
    _sumAccelX(0.0f), _sumAccelY(0.0f), _sumAccelZ(0.0f),
    _lastBMP280Reinit(0),
    _bmp280FailCount(0),
    _historyIndex(0),
    _historyFull(false),
    _lastUpdateTime(0),
    _lastPressureRead(0.0f),
    _identicalReadings(0),
    _warmupStartTime(0)
{
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0f;
        _accelYBuffer[i] = 0.0f;
        _accelZBuffer[i] = 0.0f;
    }
    
    for (int i = 0; i < 5; i++) {
        _pressureHistory[i] = 1013.25f;
        _altitudeHistory[i] = 0.0f;
        _tempHistory[i]     = 20.0f;
    }
}


bool SensorManager::begin() {
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores PION...");
    bool success = false;
    uint8_t sensorsFound = 0;

    // MPU9250 (IMU 9-axis)
    _mpu9250Online = _initMPU9250();
    if (_mpu9250Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] MPU9250: ONLINE (9-axis)");
    }

    // BMP280 (Pressão + Temp)
    _bmp280Online = _initBMP280();
    if (_bmp280Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] BMP280: ONLINE");
    }

    // SI7021 (Umidade)
    _si7021Online = _initSI7021();
    if (_si7021Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] SI7021: ONLINE");
    }

    // CCS811 (CO2 + VOC) - WARMUP LONGO
    _ccs811Online = _initCCS811();
    if (_ccs811Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] CCS811: ONLINE");
    }

    if (_mpu9250Online) {
        _calibrated = _calibrateMPU9250();
    }

    DEBUG_PRINTF("[SensorManager] %d/4 sensores detectados\n", sensorsFound);
    return success;
}

void SensorManager::update() {
    uint32_t currentTime = millis();

    if (currentTime - _lastHealthCheck >= 30000) {
        _lastHealthCheck = currentTime;
        _performHealthCheck();
    }

    if (currentTime - _lastReadTime >= SENSOR_READ_INTERVAL) {
        _lastReadTime = currentTime;
        _updateIMU();
        _updateBMP280();
        _updateSI7021();
        _updateCCS811();
        _updateTemperatureRedundancy();  // ← NOVO: Gerenciar fallback
    }
}


void SensorManager::_updateIMU() {
    if (!_mpu9250Online) return;

    xyzFloat gValues   = _mpu9250.getGValues();
    xyzFloat gyrValues = _mpu9250.getGyrValues();
    xyzFloat magValues = _mpu9250.getMagValues();

    if (!_validateMPUReadings(gyrValues.x, gyrValues.y, gyrValues.z,
                              gValues.x, gValues.y, gValues.z,
                              magValues.x, magValues.y, magValues.z)) {
        _consecutiveFailures++;
        return;
    }

    // Aplicar filtro de média móvel (otimizado)
    _accelX = _applyFilter(gValues.x, _accelXBuffer, _sumAccelX);
    _accelY = _applyFilter(gValues.y, _accelYBuffer, _sumAccelY);
    _accelZ = _applyFilter(gValues.z, _accelZBuffer, _sumAccelZ);
    
    _gyroX = gyrValues.x;
    _gyroY = gyrValues.y;
    _gyroZ = gyrValues.z;
    
    // Aplicar calibração do magnetômetro
    _magX = magValues.x - _magOffsetX;
    _magY = magValues.y - _magOffsetY;
    _magZ = magValues.z - _magOffsetZ;
    
    _consecutiveFailures = 0;
}
void SensorManager::_updateBMP280() {
    if (!_bmp280Online) {
        _temperatureBMP = NAN;
        return;
    }

    bool readSuccess = false;
    float temp = NAN, press = NAN, alt = NAN;
    
    // Retry loop
    for (int retry = 0; retry < 3; retry++) {
        if (!_waitForBMP280Measurement()) {
            delay(10);
            continue;
        }

        temp  = _bmp280.readTemperature();
        press = _bmp280.readPressure() / 100.0f;   // Pa → hPa
        alt   = _calculateAltitude(press);         // usa tua função interna

        if (!isnan(temp) && !isnan(press) && !isnan(alt)) {
            readSuccess = true;
            break;
        }
        
        if (retry < 2) {
            DEBUG_PRINTF("[SensorManager] BMP280: Retry %d (NaN detectado)\n", retry + 1);
            delay(50);
        }
    }
    
    if (!readSuccess) {
        _bmp280FailCount++;
        DEBUG_PRINTLN("[SensorManager] BMP280: Falha após 3 tentativas");
        
        if (_bmp280FailCount >= 5) {
            unsigned long now = millis();
            
            if (now - _lastBMP280Reinit > 30000) {
                DEBUG_PRINTLN("[SensorManager] BMP280: 5 falhas, FORÇANDO reinicialização...");
                _lastBMP280Reinit = now;
                
                if (_reinitBMP280()) {
                    DEBUG_PRINTLN("[SensorManager] BMP280 reinicializado!");
                } else {
                    _bmp280Online = false;
                    DEBUG_PRINTLN("[SensorManager] BMP280: Falha crítica, desabilitando");
                }
            }
        }
        return;
    }

    // Atualizar valores temporários
    float tempBackup  = _temperatureBMP;
    float pressBackup = _pressure;
    float altBackup   = _altitude;
    
    _temperatureBMP = temp;
    _pressure       = press;
    _altitude       = alt;
    
    // Validar ANTES de aceitar
    if (!_validateBMP280Reading()) {
        // Restaurar valores anteriores
        _temperatureBMP = tempBackup;
        _pressure       = pressBackup;
        _altitude       = altBackup;
        
        _bmp280FailCount++;
        _bmp280TempFailures++;
        
        DEBUG_PRINTF("[SensorManager] BMP280: Leitura rejeitada (P=%.0f vs anterior %.0f hPa)\n", 
                     press, pressBackup);
        
        bool bigDifference = fabsf(press - pressBackup) > 50.0f;
        bool sensorFrozen  = (_identicalReadings >= 10);
        
        if (bigDifference || sensorFrozen) {
            unsigned long now = millis();
            
            if (now - _lastBMP280Reinit > 10000) {
                if (sensorFrozen) {
                    DEBUG_PRINTLN("[SensorManager] BMP280: SENSOR TRAVADO! Forçando reinicialização IMEDIATA...");
                } else {
                    DEBUG_PRINTLN("[SensorManager] BMP280: Diferença grande detectada, forçando reinicialização...");
                }
                
                _lastBMP280Reinit = now;
                
                if (_reinitBMP280()) {
                    DEBUG_PRINTLN("[SensorManager] BMP280: Reinicializado com sucesso!");
                    _bmp280FailCount    = 0;
                    _bmp280TempFailures = 0;
                    _identicalReadings  = 0;
                } else {
                    DEBUG_PRINTLN("[SensorManager] BMP280: FALHA CRÍTICA na reinicialização!");
                    _bmp280Online = false;
                }
                return;
            }
        }
        
        if (_bmp280FailCount >= 5) {
            unsigned long now = millis();
            
            if (now - _lastBMP280Reinit > 30000) {
                DEBUG_PRINTLN("[SensorManager] BMP280: 5 falhas, reinicializando...");
                _lastBMP280Reinit = now;
                
                if (_reinitBMP280()) {
                    DEBUG_PRINTLN("[SensorManager] BMP280 reinicializado!");
                } else {
                    _bmp280Online = false;
                }
            }
        }
        return;
    }

    // Leitura válida e aceita
    _bmp280TempValid    = true;
    _bmp280FailCount    = 0;
    _bmp280TempFailures = 0;
}


void SensorManager::forceReinitBMP280() {
    DEBUG_PRINTLN("[SensorManager] Reinicialização forçada do BMP280...");
    _bmp280Online = _initBMP280();
}

bool SensorManager::_reinitBMP280() {
    return _initBMP280();  // Usa o método init aprimorado
}

void SensorManager::_updateSI7021() {
    if (!_si7021Online) return;
    
    uint32_t currentTime = millis();
    if (currentTime - _lastSI7021Read < SI7021_READ_INTERVAL) return;
    
    bool humiditySuccess = false;

    // Passo 1: Umidade
    float hum = NAN;
    if (_si7021.readHumidity(hum)) {
        if (!isnan(hum) && hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
            _humidity        = hum;
            _lastSI7021Read  = currentTime;
            humiditySuccess  = true;
        }
    }

    if (!humiditySuccess) {
        static uint8_t failCount = 0;
        failCount++;
        
        if (failCount >= 10) {
            DEBUG_PRINTLN("[SensorManager] SI7021: 10 falhas consecutivas (umidade)");
            failCount = 0;
        }
        return;
    }

    // Passo 2: Temperatura
    float temp = NAN;
    if (_si7021.readTemperature(temp)) {
        if (_validateReading(temp, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
            _temperatureSI      = temp;
            _si7021TempValid    = true;
            _si7021TempFailures = 0;
        } else {
            _si7021TempValid = false;
            _si7021TempFailures++;
            
            if (_si7021TempFailures >= MAX_TEMP_FAILURES) {
                DEBUG_PRINTLN("[SensorManager] SI7021: Temperatura com falhas consecutivas");
            }
        }
    }
}


void SensorManager::_updateCCS811() {
    if (!_ccs811Online) return;

    uint32_t currentTime = millis();
    if (currentTime - _lastCCS811Read < CCS811_READ_INTERVAL) return;

    _lastCCS811Read = currentTime;

    if (_ccs811.available() && _ccs811.readData() == 0) {
        float co2  = _ccs811.geteCO2();
        float tvoc = _ccs811.getTVOC();

        if (_validateCCSReadings(co2, tvoc)) {
            _co2Level = co2;
            _tvoc     = tvoc;
        }
    }
}

// Getters
float SensorManager::getTemperature() { return _temperature; }
float SensorManager::getTemperatureSI7021() { return _temperatureSI; }
float SensorManager::getTemperatureBMP280() { return _temperatureBMP; }
float SensorManager::getPressure() { return _pressure; }
float SensorManager::getAltitude() { return _altitude; }
float SensorManager::getGyroX() { return _gyroX; }
float SensorManager::getGyroY() { return _gyroY; }
float SensorManager::getGyroZ() { return _gyroZ; }
float SensorManager::getAccelX() { return _accelX; }
float SensorManager::getAccelY() { return _accelY; }
float SensorManager::getAccelZ() { return _accelZ; }
float SensorManager::getAccelMagnitude() {
    return sqrt(_accelX*_accelX + _accelY*_accelY + _accelZ*_accelZ);
}
float SensorManager::getMagX() { return _magX; }
float SensorManager::getMagY() { return _magY; }
float SensorManager::getMagZ() { return _magZ; }
float SensorManager::getHumidity() { return _humidity; }
float SensorManager::getCO2() { return _co2Level; }
float SensorManager::getTVOC() { return _tvoc; }

bool SensorManager::isMPU9250Online() { return _mpu9250Online; }
bool SensorManager::isBMP280Online() { return _bmp280Online; }
bool SensorManager::isSI7021Online() { return _si7021Online; }
bool SensorManager::isCCS811Online() { return _ccs811Online; }
bool SensorManager::isCalibrated() { return _calibrated; }

void SensorManager::getRawData(float& gx, float& gy, float& gz,
                               float& ax, float& ay, float& az) {
    gx = _gyroX; gy = _gyroY; gz = _gyroZ;
    ax = _accelX; ay = _accelY; az = _accelZ;
}

void SensorManager::printSensorStatus() {
    DEBUG_PRINTF("  MPU9250: %s\n", _mpu9250Online ? "ONLINE (9-axis)" : "offline");
    DEBUG_PRINTF("  BMP280:  %s", _bmp280Online ? "ONLINE" : "offline");
    
    if (_bmp280Online) {
        DEBUG_PRINTF(" (Temp: %s)", _bmp280TempValid ? "OK" : "FALHA");
    }
    DEBUG_PRINTLN();
    
    DEBUG_PRINTF("  SI7021:  %s", _si7021Online ? "ONLINE" : "offline");
    
    if (_si7021Online) {
        DEBUG_PRINTF(" (Temp: %s)", _si7021TempValid ? "OK" : "FALHA");
    }
    DEBUG_PRINTLN();
    
    DEBUG_PRINTF("  CCS811:  %s\n", _ccs811Online ? "ONLINE" : "offline");
    
    // Status de redundância
    DEBUG_PRINTLN("\n  Redundância de Temperatura:");
    if (_si7021TempValid) {
        DEBUG_PRINTF("    Usando SI7021 (%.2f°C)\n", _temperatureSI);
    } else if (_bmp280TempValid) {
        DEBUG_PRINTF("    Usando BMP280 (%.2f°C) - SI7021 falhou\n", _temperatureBMP);
    } else {
        DEBUG_PRINTLN("    CRÍTICO: Ambos sensores falharam!");
    }
}


void SensorManager::resetAll() {
    _mpu9250Online = _initMPU9250();
    _bmp280Online = _initBMP280();
    _si7021Online = _initSI7021();
    _ccs811Online = _initCCS811();
    _consecutiveFailures = 0;
}

bool SensorManager::_initMPU9250() {
    if (!_i2c) return false;

    // Inicialização básica da IMU via HAL (acc + gyro)
    if (!_mpu9250.begin()) {
        return false;
    }

    // Inicializar magnetômetro (AK8963 interno)
    if (!_mpu9250.initMagnetometer()) {
        DEBUG_PRINTLN("[SensorManager] Magnetometro falhou");
    } else {
        DEBUG_PRINTLN("[SensorManager] Magnetometro OK, iniciando calibração...");
        
        float magMin[3] = {9999.0f, 9999.0f, 9999.0f};
        float magMax[3] = {-9999.0f, -9999.0f, -9999.0f};
        
        DEBUG_PRINTLN("[SensorManager] Rotacione o CubeSat lentamente em todos os eixos...");
        
        uint32_t startTime = millis();
        uint16_t samples   = 0;
        const uint32_t calibrationTime = 10000;  // 10 segundos
        
        while (millis() - startTime < calibrationTime) {
            xyzFloat mag = _mpu9250.getMagValues();
            
            if (!isnan(mag.x) && !isnan(mag.y) && !isnan(mag.z)) {
                magMin[0] = min(magMin[0], mag.x);
                magMin[1] = min(magMin[1], mag.y);
                magMin[2] = min(magMin[2], mag.z);
                
                magMax[0] = max(magMax[0], mag.x);
                magMax[1] = max(magMax[1], mag.y);
                magMax[2] = max(magMax[2], mag.z);
                
                samples++;
            }
            
            static uint32_t lastDisplayUpdate = 0;
            if (millis() - lastDisplayUpdate >= 100) {
                lastDisplayUpdate = millis();
                
                uint8_t progress =
                    ((millis() - startTime) * 100) / calibrationTime;
                
                extern DisplayManager* g_displayManagerPtr;
                if (g_displayManagerPtr && g_displayManagerPtr->isOn()) {
                    g_displayManagerPtr->showCalibration(progress);
                }
                
                if ((millis() - startTime) % 2000 < 100) {
                    DEBUG_PRINTF(
                        "[SensorManager] Calibrando... %lus / 10s (%d samples)\n", 
                        (millis() - startTime) / 1000, samples
                    );
                }
            }
            
            delay(50);
        }
        
        if (samples > 100) {
            _magOffsetX = (magMax[0] + magMin[0]) / 2.0f;
            _magOffsetY = (magMax[1] + magMin[1]) / 2.0f;
            _magOffsetZ = (magMax[2] + magMin[2]) / 2.0f;
            
            DEBUG_PRINTLN("[SensorManager] Magnetometro calibrado!");
            DEBUG_PRINTF("[SensorManager] Offsets: X=%.2f Y=%.2f Z=%.2f µT\n", 
                         _magOffsetX, _magOffsetY, _magOffsetZ);
            DEBUG_PRINTF("[SensorManager] Samples coletados: %d\n", samples);
            
            extern DisplayManager* g_displayManagerPtr;
            if (g_displayManagerPtr && g_displayManagerPtr->isOn()) {
                g_displayManagerPtr->showCalibrationResult(
                    _magOffsetX, _magOffsetY, _magOffsetZ
                );
            }
        } else {
            DEBUG_PRINTLN("[SensorManager] Calibração insuficiente, usando offsets zero");
            _magOffsetX = 0.0f;
            _magOffsetY = 0.0f;
            _magOffsetZ = 0.0f;
        }
    }
    
    delay(100);
    xyzFloat testRead = _mpu9250.getGValues();
    return !isnan(testRead.x);
}


bool SensorManager::_waitForBMP280Measurement() {
    if (!_i2c || !_bmp280Online) return false;

    const uint8_t BMP280_STATUS_REG = 0xF3;   // STATUS [web:102]
    const uint8_t STATUS_MEASURING  = 0x08;   // bit 3
    const uint8_t BMP280_ADDR       = BMP280_ADDR_1;  // ou se você guardar o endereço detectado

    uint8_t maxRetries = 50;  // ~50 ms de timeout

    while (maxRetries--) {
        // Lê STATUS via HAL::I2C em vez de Wire
        uint8_t status = _i2c->readRegister(BMP280_ADDR, BMP280_STATUS_REG);

        // Bit 3 = 0 significa medição completa (datasheet BMP280) [web:102]
        if ((status & STATUS_MEASURING) == 0) {
            return true;
        }

        delay(1);
    }

    return false;
}




float SensorManager::_getMedian(float* values, uint8_t count) {
    float sorted[5];
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

bool SensorManager::_isOutlier(float value, float* history, uint8_t count) {
    if (!_historyFull && _historyIndex < 3) {
        return false;  // Histórico insuficiente
    }
    
    float median = _getMedian(history, count);
    
    float deviations[5];
    for (uint8_t i = 0; i < count; i++) {
        deviations[i] = abs(history[i] - median);
    }
    
    float mad = _getMedian(deviations, count);
    if (mad < 0.1) mad = 0.1;
    
    float score = abs(value - median) / mad;
    
    return (score > 3.0);  // Outlier se > 3 MAD
}

bool SensorManager::_validateBMP280Reading() {
    if (!_bmp280Online) return false;
    
    float temp = _temperatureBMP;
    float press = _pressure;
    float alt = _altitude;
    
    // Validação básica
    if (isnan(temp) || isnan(press) || isnan(alt)) {
        return false;
    }
    
    // ✅ DETECTOR DE TRAVAMENTO CORRIGIDO
    // Detectar apenas se valores são BITWISE IDÊNTICOS (não apenas próximos)
    // Usar memcmp para comparação exata de bits
    bool exactlyIdentical = (memcmp(&press, &_lastPressureRead, sizeof(float)) == 0);
    
    if (exactlyIdentical && _lastPressureRead != 0.0) {
        _identicalReadings++;
        
        // ✅ Aumentar threshold: 50 leituras idênticas (não 10)
        // Isso significa ~50 segundos de valores EXATAMENTE iguais
        if (_identicalReadings >= 50) {
            DEBUG_PRINTF("[SensorManager] BMP280: TRAVADO! (P=%.2f hPa por %d leituras EXATAS)\n", 
                        press, _identicalReadings);
            _identicalReadings = 0;
            return false;
        }
    } else {
        _identicalReadings = 0;
    }
    _lastPressureRead = press;
    
    unsigned long now = millis();
    float deltaTime = (now - _lastUpdateTime) / 1000.0;
    
    // Validação de taxa de mudança
    if (_lastUpdateTime > 0 && deltaTime > 0.1 && deltaTime < 10.0) {
        
        // Pressão: max 20 hPa/s
        float pressRate = abs(press - _pressureHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (pressRate > 20.0) {
            DEBUG_PRINTF("[SensorManager] BMP280: Taxa pressão anormal: %.1f hPa/s\n", pressRate);
            return false;
        }
        
        // Altitude: max 150 m/s
        float altRate = abs(alt - _altitudeHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (altRate > 150.0) {
            DEBUG_PRINTF("[SensorManager] BMP280: Taxa altitude anormal: %.1f m/s\n", altRate);
            return false;
        }
        
        // Temperatura: max 0.1°C/s
        float tempRate = abs(temp - _tempHistory[(_historyIndex + 4) % 5]) / deltaTime;
        if (tempRate > 0.1) {
            DEBUG_PRINTF("[SensorManager] BMP280: Taxa temp anormal: %.2f°C/s\n", tempRate);
            return false;
        }
    }
    
    // Warm-up period
    if (_warmupStartTime == 0) {
        _warmupStartTime = millis();
    }
    
    unsigned long warmupElapsed = millis() - _warmupStartTime;
    
    if (warmupElapsed < 30000) {
        // Durante warm-up, não aplicar detecção de outliers
        DEBUG_PRINTF("[SensorManager] BMP280: Warm-up (%lus/30s)\n", warmupElapsed / 1000);
    } else {
        // Após warm-up, aplicar detecção de outliers
        if (_historyFull || _historyIndex >= 3) {
            uint8_t histCount = _historyFull ? 5 : _historyIndex;
            
            if (_isOutlier(press, _pressureHistory, histCount)) {
                DEBUG_PRINTF("[SensorManager] BMP280: Pressão outlier: %.0f hPa\n", press);
                return false;
            }
        }
    }
    
    // Validação cruzada com SI7021 (relaxada)
    if (_si7021Online && _si7021TempValid) {
        float tempDelta = abs(temp - _temperatureSI);
        float threshold = 4.0 + (alt / 10000.0);
        
        if (tempDelta > threshold) {
            DEBUG_PRINTF("[SensorManager] BMP280: Delta temp: %.1f°C (limite: %.1f°C)\n",
                        tempDelta, threshold);
            
            if (tempDelta > 5.0) {
                DEBUG_PRINTLN("[SensorManager] BMP280: Delta crítico!");
                return false;
            }
            
            DEBUG_PRINTLN("[SensorManager] BMP280: Delta alto mas aceitável");
        }
    }
    
    // Atualizar histórico
    _pressureHistory[_historyIndex] = press;
    _altitudeHistory[_historyIndex] = alt;
    _tempHistory[_historyIndex] = temp;
    
    _historyIndex = (_historyIndex + 1) % 5;
    if (_historyIndex == 0) _historyFull = true;
    
    _lastUpdateTime = now;
    
    return true;
}



bool SensorManager::_initBMP280() {
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] Inicializando BMP280 (método robusto)");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    _bmp280Online      = false;
    _bmp280TempValid   = false;
    
    _warmupStartTime   = 0;
    _identicalReadings = 0;
    _lastPressureRead  = 0.0f;
    
    delay(50);
    
    // Tentar 0x76 / 0x77 usando o driver HAL
    uint8_t addresses[] = {BMP280_ADDR_1, BMP280_ADDR_2};
    bool found = false;
    
    for (uint8_t addr : addresses) {
        DEBUG_PRINTF("[SensorManager] Tentando BMP280 em 0x%02X...\n", addr);
        
        for (int attempt = 0; attempt < 5; attempt++) {
            if (_bmp280.begin(addr)) {
                found = true;
                DEBUG_PRINTF("[SensorManager] BMP280 detectado em 0x%02X (tentativa %d)\n", 
                             addr, attempt + 1);
                break;
            }
            delay(200);
        }
        if (found) break;
    }
    
    if (!found) {
        DEBUG_PRINTLN("[SensorManager] BMP280 não detectado");
        return false;
    }
    
    DEBUG_PRINTLN("[SensorManager] Configuração aplicada (via BMP280Hal)");
    
    // Warm-up global (filtro IIR + estabilidade)
    DEBUG_PRINTLN("[SensorManager] Aguardando estabilização (5 segundos)...");
    delay(5000);
    
    DEBUG_PRINTLN("[SensorManager] Testando leituras (5 ciclos com retry)...");
    
    for (int i = 0; i < 5; i++) {
        bool readSuccess = false;
        float press = NAN, temp = NAN;
        
        for (int retry = 0; retry < 3; retry++) {
            if (_waitForBMP280Measurement()) {
                press = _bmp280.readPressure() / 100.0f;   // Pa → hPa
                temp  = _bmp280.readTemperature();
                
                if (!isnan(press) && !isnan(temp)) {
                    readSuccess = true;
                    break;
                }
            }
            delay(50);
        }
        
        if (!readSuccess) {
            DEBUG_PRINTF("[SensorManager] Falha na leitura %d após 3 tentativas\n", i + 1);
            return false;
        }
        
        DEBUG_PRINTF("[SensorManager]   Leitura %d: T=%.1f°C P=%.0f hPa\n",
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
    
    _bmp280Online      = true;
    _bmp280TempValid   = true;
    _bmp280FailCount   = 0;
    
    DEBUG_PRINTLN("[SensorManager] ========================================");
    DEBUG_PRINTLN("[SensorManager] BMP280 INICIALIZADO COM SUCESSO!");
    DEBUG_PRINTLN("[SensorManager] ========================================");
    
    return true;
}


bool SensorManager::_initSI7021() {
    DEBUG_PRINTLN("[SensorManager] Inicializando SI7021 (driver HAL)...");
    
    _si7021Online      = false;
    _si7021TempValid   = false;
    _si7021TempFailures= 0;

    // Testar presença + soft reset + user register via driver
    if (!_si7021.begin(SI7021_ADDRESS)) {
        DEBUG_PRINTLN("[SensorManager] SI7021: Não inicializado (HAL)");
        return false;
    }

    // Fazer uma leitura inicial para log
    float hum = NAN;
    float temp = NAN;

    if (_si7021.readHumidity(hum)) {
        _humidity = hum;
        DEBUG_PRINTF("[SensorManager] SI7021: Umidade inicial = %.1f%%\n", hum);
    }

    if (_si7021.readTemperature(temp)) {
        if (_validateReading(temp, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
            _temperatureSI      = temp;
            _si7021TempValid    = true;
            _si7021TempFailures = 0;
            DEBUG_PRINTF("[SensorManager] SI7021: Temperatura inicial = %.2f°C\n", temp);
        }
    }

    _si7021Online = true;
    return true;
}


bool SensorManager::_initCCS811() {
    DEBUG_PRINTLN("[SensorManager] Inicializando CCS811 (HAL)...");

    if (!_ccs811.begin(CCS811_ADDR_1)) {
        DEBUG_PRINTLN("[SensorManager] CCS811: Falha no begin()");
        return false;
    }

    DEBUG_PRINTLN("[SensorManager] CCS811: Aguardando warmup (20s)...");
    uint32_t startTime = millis();

    while (!_ccs811.available() && (millis() - startTime < CCS811_WARMUP_TIME)) {
        delay(500);
        if ((millis() - startTime) % 5000 == 0) {
            DEBUG_PRINTF("[SensorManager] Warmup: %lus / 20s\n",
                         (millis() - startTime) / 1000);
        }
    }

    if (!_ccs811.available()) {
        DEBUG_PRINTLN("[SensorManager] CCS811: Timeout warmup");
        return false;
    }

    DEBUG_PRINTLN("[SensorManager] CCS811 disponível!");

    // Compensação ambiental usando BMP280Hal + SI7021Hal
    if (_bmp280Online || _si7021Online) {
        float temp = 25.0f;
        float hum  = 50.0f;

        if (_bmp280Online) {
            float t = _bmp280.readTemperature();
            if (_validateReading(t, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
                temp = t;
            }
        }

        if (_si7021Online) {
            float h = NAN;
            if (_si7021.readHumidity(h) &&
                h >= HUMIDITY_MIN_VALID && h <= HUMIDITY_MAX_VALID) {
                hum = h;
            }
        }

        _ccs811.setEnvironmentalData(hum, temp);
        DEBUG_PRINTF("[SensorManager] CCS811: Compensação T=%.1f°C H=%.1f%%\n", temp, hum);
    }

    return true;
}

bool SensorManager::_validateMPUReadings(float gx, float gy, float gz,
                                          float ax, float ay, float az,
                                          float mx, float my, float mz) {
    if (isnan(gx) || isnan(gy) || isnan(gz)) return false;
    if (isnan(ax) || isnan(ay) || isnan(az)) return false;
    if (isnan(mx) || isnan(my) || isnan(mz)) return false;
    
    if (abs(gx) > 2000 || abs(gy) > 2000 || abs(gz) > 2000) return false;
    if (abs(ax) > 16 || abs(ay) > 16 || abs(az) > 16) return false;
    if (mx < MAG_MIN_VALID || mx > MAG_MAX_VALID) return false;
    if (my < MAG_MIN_VALID || my > MAG_MAX_VALID) return false;
    if (mz < MAG_MIN_VALID || mz > MAG_MAX_VALID) return false;
    
    return true;
}

bool SensorManager::_validateReading(float value, float minValid, float maxValid) {
    if (isnan(value)) return false;
    if (value < minValid || value > maxValid) return false;
    
    // Detectar valores absurdos (sensor morto retornando 0 ou -273.15°C)
    if (value == 0.0 || value == -273.15) return false;
    
    return true;
}

bool SensorManager::_validateCCSReadings(float co2, float tvoc) {
    if (isnan(co2) || isnan(tvoc)) return false;
    if (co2 < CO2_MIN_VALID || co2 > CO2_MAX_VALID) return false;
    if (tvoc < TVOC_MIN_VALID || tvoc > TVOC_MAX_VALID) return false;
    return true;
}

void SensorManager::_performHealthCheck() {
    if (_consecutiveFailures >= 10) {
        DEBUG_PRINTLN("[SensorManager] Health check: Resetando...");
        resetAll();
        _consecutiveFailures = 5;
    }
    
    // ========== NOVO: Recuperação de sensores de temperatura ==========
    
    // Tentar recuperar SI7021 se falhou muitas vezes
    if (_si7021Online && _si7021TempFailures >= MAX_TEMP_FAILURES) {
        DEBUG_PRINTLN("[SensorManager] Tentando recuperar SI7021...");
        _si7021Online = _initSI7021();
        
        if (_si7021Online) {
            _si7021TempFailures = 0;
            _si7021TempValid = false;
            DEBUG_PRINTLN("[SensorManager] SI7021 recuperado!");
        }
    }
    
    // Tentar recuperar BMP280 se falhou muitas vezes
    if (_bmp280Online && _bmp280TempFailures >= MAX_TEMP_FAILURES) {
        DEBUG_PRINTLN("[SensorManager] Tentando recuperar BMP280...");
        _bmp280Online = _initBMP280();
        
        if (_bmp280Online) {
            _bmp280TempFailures = 0;
            _bmp280TempValid = false;
            DEBUG_PRINTLN("[SensorManager] BMP280 recuperado!");
        }
    }
}


bool SensorManager::_calibrateMPU9250() {
    DEBUG_PRINTLN("[SensorManager] Calibrando MPU9250 (via _initMPU9250)...");
    
    bool ok = _initMPU9250();
    _calibrated = ok;
    
    if (ok) {
        DEBUG_PRINTLN("[SensorManager] Calibração concluída!");
    } else {
        DEBUG_PRINTLN("[SensorManager] Falha na calibração da MPU9250");
    }
    return ok;
}


// OTIMIZADO: Multiplicação ao invés de divisão
float SensorManager::_applyFilter(float newValue, float* buffer, float& sum) {
    sum -= buffer[_filterIndex];
    buffer[_filterIndex] = newValue;
    sum += newValue;
    _filterIndex = (_filterIndex + 1) % CUSTOM_FILTER_SIZE;
    
    // Multiplicação é 4x mais rápida que divisão
    constexpr float INV_SIZE = 1.0f / CUSTOM_FILTER_SIZE;
    return sum * INV_SIZE;
}

float SensorManager::_calculateAltitude(float pressure) {
    if (pressure <= 0.0) return 0.0;
    float ratio = pressure / _seaLevelPressure;
    return 44330.0 * (1.0 - pow(ratio, 0.1903));
}

bool SensorManager::calibrateIMU() {
    return _calibrateMPU9250();
}

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("[SensorManager] Scanning I2C bus via HAL...");
    if (!_i2c) return;

    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        // Heurística simples: tentar ler reg 0x00; se não travar, assume presente
        uint8_t val = _i2c->readRegister(addr, 0x00);
        // Não temos status, então só logar tudo que responder
        DEBUG_PRINTF("  Device at 0x%02X (val=0x%02X)\n", addr, val);
        count++;
    }

    DEBUG_PRINTF("[SensorManager] Found %d devices\n", count);
}


void SensorManager::_updateTemperatureRedundancy() {
    // Prioridade 1: SI7021
    if (_si7021Online && _si7021TempValid && 
        _validateReading(_temperatureSI, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
        _temperature = _temperatureSI;
        return;
    }
    
    // Fallback 2: BMP280
    if (_bmp280Online && _bmp280TempValid && 
        _validateReading(_temperatureBMP, TEMP_MIN_VALID, TEMP_MAX_VALID)) {
        _temperature = _temperatureBMP;
        
        static bool lastUsedBMP = false;
        if (!lastUsedBMP) {
            DEBUG_PRINTLN("[SensorManager] Temperatura: Usando BMP280 (SI7021 indisponível)");
            lastUsedBMP = true;
        }
        return;
    }
    
    // Ambos falharam
    _temperature = NAN;
    
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning > 30000) {
        lastWarning = millis();
        DEBUG_PRINTLN("[SensorManager] CRÍTICO: Ambos sensores de temperatura falharam!");
    }
}
