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

SensorManager::SensorManager() :
    _mpu9250(MPU9250_ADDRESS),
    _si7021(),
    _temperature(NAN), _temperatureBMP(NAN), _temperatureSI(NAN), 
    _pressure(NAN), _altitude(NAN),
    _humidity(NAN), _co2Level(NAN), _tvoc(NAN),
    _seaLevelPressure(1013.25),
    _gyroX(0.0), _gyroY(0.0), _gyroZ(0.0),
    _accelX(0.0), _accelY(0.0), _accelZ(0.0),
    _magX(0.0), _magY(0.0), _magZ(0.0),
    _magOffsetX(0.0), _magOffsetY(0.0), _magOffsetZ(0.0),
    _mpu9250Online(false), _bmp280Online(false),
    _si7021Online(false), _ccs811Online(false),
    _calibrated(false),
    _si7021TempValid(false), _bmp280TempValid(false),  // ← NOVO
    _si7021TempFailures(0), _bmp280TempFailures(0),    // ← NOVO
    _lastReadTime(0), _lastCCS811Read(0), _lastSI7021Read(0),
    _lastHealthCheck(0), _consecutiveFailures(0), _filterIndex(0),
    _sumAccelX(0), _sumAccelY(0), _sumAccelZ(0)
{
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0;
        _accelYBuffer[i] = 0.0;
        _accelZBuffer[i] = 0.0;
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

    xyzFloat gValues = _mpu9250.getGValues();
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

    float temp = _bmp280.readTemperature();
    float press = _bmp280.readPressure();

    // Validar temperatura e pressão separadamente
    bool tempValid = !isnan(temp) && temp >= TEMP_MIN_VALID && temp <= TEMP_MAX_VALID;
    bool pressValid = !isnan(press) && press >= PRESSURE_MIN_VALID * 100 && press <= PRESSURE_MAX_VALID * 100;

    if (tempValid) {
        _temperatureBMP = temp;
    } else {
        _temperatureBMP = NAN;
    }

    if (pressValid) {
        _pressure = press / 100.0;
        _altitude = _calculateAltitude(_pressure);
    }
}



void SensorManager::_updateSI7021() {
    if (!_si7021Online) return;
    
    uint32_t currentTime = millis();
    if (currentTime - _lastSI7021Read < SI7021_READ_INTERVAL) return;
    
    // ========================================
    // PASSO 1: Ler UMIDADE (comando 0xF5)
    // ========================================
    Wire.beginTransmission(SI7021_ADDRESS);
    Wire.write(0xF5);
    uint8_t error = Wire.endTransmission();
    if (error != 0) return;
    
    delay(50);
    
    bool humiditySuccess = false;
    
    delay(50);  // Total 100ms após comando (garante conversão completa)
    
    Wire.requestFrom((uint8_t)SI7021_ADDRESS, (uint8_t)3);
    
    if (Wire.available() >= 2) {
        uint8_t msb = Wire.read();
        uint8_t lsb = Wire.read();
        if (Wire.available()) Wire.read();
        
        uint16_t rawHum = (msb << 8) | lsb;
        
        if (rawHum != 0xFFFF && rawHum != 0x0000) {
            float hum = ((125.0 * rawHum) / 65536.0) - 6.0;
            
            if (hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
                _humidity = hum;
                _lastSI7021Read = currentTime;
                humiditySuccess = true;
            }
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
    
    // ========================================
    // PASSO 2: Ler TEMPERATURA (comando 0xF3)
    // ========================================
    
delay(30);

Wire.beginTransmission(SI7021_ADDRESS);
Wire.write(0xF3);
error = Wire.endTransmission();
if (error != 0) return;

delay(80);  // Tempo total garantido para conversão

Wire.requestFrom((uint8_t)SI7021_ADDRESS, (uint8_t)2);

if (Wire.available() >= 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    
    uint16_t rawTemp = (msb << 8) | lsb;
    
    if (rawTemp != 0xFFFF && rawTemp != 0x0000) {
        float temp = ((175.72 * rawTemp) / 65536.0) - 46.85;
        
        if (_validateTemperature(temp)) {
            _temperatureSI = temp;
            _si7021TempValid = true;
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
}

void SensorManager::_updateCCS811() {
    if (!_ccs811Online) return;

    uint32_t currentTime = millis();
    if (currentTime - _lastCCS811Read < CCS811_READ_INTERVAL) return;
    
    _lastCCS811Read = currentTime;

    if (_ccs811.available() && !_ccs811.readData()) {
        float co2 = _ccs811.geteCO2();
        float tvoc = _ccs811.getTVOC();

        if (_validateCCSReadings(co2, tvoc)) {
            _co2Level = co2;
            _tvoc = tvoc;
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
    Wire.beginTransmission(MPU9250_ADDRESS);
    if (Wire.endTransmission() != 0) return false;

    if (!_mpu9250.init()) return false;

    _mpu9250.setAccRange(MPU9250_ACC_RANGE_8G);
    _mpu9250.setGyrRange(MPU9250_GYRO_RANGE_500);
    _mpu9250.enableGyrDLPF();
    _mpu9250.setGyrDLPF(MPU9250_DLPF_6);
    
    // Inicializar magnetômetro
    if (!_mpu9250.initMagnetometer()) {
        DEBUG_PRINTLN("[SensorManager] Magnetometro falhou");
    } else {
        DEBUG_PRINTLN("[SensorManager] Magnetometro OK, iniciando calibração...");
        
        // Calibração manual de magnetômetro (hard iron)
        // Coletar min/max durante 10 segundos
        float magMin[3] = {9999.0, 9999.0, 9999.0};
        float magMax[3] = {-9999.0, -9999.0, -9999.0};
        
        DEBUG_PRINTLN("[SensorManager] Rotacione o CubeSat lentamente em todos os eixos...");
        uint32_t startTime = millis();
        uint16_t samples = 0;
        
        while (millis() - startTime < 10000) {  // 10 segundos
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
            
            delay(50);
            
            // Feedback visual
            if ((millis() - startTime) % 2000 == 0) {
                DEBUG_PRINTF("[SensorManager] Calibrando... %lus / 10s (%d samples)\n", 
                            (millis() - startTime) / 1000, samples);
            }
        }
        
        // Calcular offsets (hard iron compensation)
        if (samples > 100) {
            _magOffsetX = (magMax[0] + magMin[0]) / 2.0;
            _magOffsetY = (magMax[1] + magMin[1]) / 2.0;
            _magOffsetZ = (magMax[2] + magMin[2]) / 2.0;
            
            DEBUG_PRINTF("[SensorManager] Magnetometro calibrado!\n");
            DEBUG_PRINTF("[SensorManager] Offsets: X=%.2f Y=%.2f Z=%.2f µT\n", 
                        _magOffsetX, _magOffsetY, _magOffsetZ);
            DEBUG_PRINTF("[SensorManager] Samples coletados: %d\n", samples);
        } else {
            DEBUG_PRINTLN("[SensorManager] Calibração insuficiente, usando offsets zero");
            _magOffsetX = 0.0;
            _magOffsetY = 0.0;
            _magOffsetZ = 0.0;
        }
    }
    
    delay(100);

    xyzFloat testRead = _mpu9250.getGValues();
    return !isnan(testRead.x);
}

bool SensorManager::_initBMP280() {
    uint8_t addresses[] = {BMP280_ADDR_1, BMP280_ADDR_2};
    
    for (uint8_t i = 0; i < 2; i++) {
        if (_bmp280.begin(addresses[i])) {
            _bmp280.setSampling(
                Adafruit_BMP280::MODE_NORMAL,
                Adafruit_BMP280::SAMPLING_X16,
                Adafruit_BMP280::SAMPLING_X16,
                Adafruit_BMP280::FILTER_X16,
                Adafruit_BMP280::STANDBY_MS_500
            );
            
            delay(100);
            float testTemp = _bmp280.readTemperature();
            
            if (!isnan(testTemp) && testTemp > TEMP_MIN_VALID && testTemp < TEMP_MAX_VALID) {
                return true;
            }
        }
    }
    return false;
}

bool SensorManager::_initSI7021() {
    DEBUG_PRINTLN("[SensorManager] Inicializando SI7021 (Wire.h puro)...");
    
    // Verificar presença física
    Wire.beginTransmission(SI7021_ADDRESS);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        DEBUG_PRINTF("[SensorManager] SI7021: Não detectado (erro %d)\n", error);
        return false;
    }
    
    DEBUG_PRINTLN("[SensorManager] SI7021: Detectado no barramento I2C");
    
    // Reset software
    Wire.beginTransmission(SI7021_ADDRESS);
    Wire.write(0xFE);  // Software Reset
    Wire.endTransmission();
    delay(50);
    
    // Configurar User Register: RH 12-bit, Temp 14-bit
    Wire.beginTransmission(SI7021_ADDRESS);
    Wire.write(0xE6);  // Write User Register
    Wire.write(0x00);  // Resolução padrão (RH 12-bit, Temp 14-bit)
    Wire.endTransmission();
    delay(20);
    
    // Testar leitura de umidade
    Wire.beginTransmission(SI7021_ADDRESS);
    Wire.write(0xF5);  // Measure RH, No Hold Master Mode
    error = Wire.endTransmission();
    
    if (error != 0) {
        DEBUG_PRINTF("[SensorManager] SI7021: Erro ao iniciar medição (erro %d)\n", error);
        return false;
    }
    
    // Aguardar conversão (máximo 12ms para RH 12-bit segundo datasheet)
    delay(20);
    
    // Ler resultado com polling
    bool success = false;
    
    for (uint8_t retry = 0; retry < 20; retry++) {
        Wire.requestFrom((uint8_t)SI7021_ADDRESS, (uint8_t)3);
        
        if (Wire.available() >= 2) {
            uint8_t msb = Wire.read();
            uint8_t lsb = Wire.read();
            
            if (Wire.available()) {
                Wire.read();  // CRC (ignorar por enquanto)
            }
            
            uint16_t rawHum = (msb << 8) | lsb;
            
            // Verificar se não é valor de erro/busy
            if (rawHum != 0xFFFF && rawHum != 0x0000) {
                // Converter para % RH (datasheet fórmula)
                float hum = ((125.0 * rawHum) / 65536.0) - 6.0;
                
                if (hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
                    DEBUG_PRINTF("[SensorManager] SI7021: OK (%.1f%% RH)\n", hum);
                    DEBUG_PRINTLN("[SensorManager] Implementação: Wire.h puro (sem biblioteca)");
                    success = true;
                    break;
                }
            }
        }
        
        delay(10);
    }
    
    if (!success) {
        DEBUG_PRINTLN("[SensorManager] SI7021: Timeout após 20 tentativas");
        DEBUG_PRINTLN("[SensorManager] Sensor detectado mas não fornece dados válidos");
        DEBUG_PRINTLN("[SensorManager] Possível chip falso/defeituoso");
    }
    
    return success;
}

bool SensorManager::_initCCS811() {
    DEBUG_PRINTLN("[SensorManager] Inicializando CCS811...");
    
    uint8_t addresses[] = {CCS811_ADDR_1, CCS811_ADDR_2};
    
    for (uint8_t i = 0; i < 2; i++) {
        DEBUG_PRINTF("[SensorManager] Testando CCS811 em 0x%02X\n", addresses[i]);
        
        Wire.beginTransmission(addresses[i]);
        uint8_t error = Wire.endTransmission();
        
        if (error != 0) {
            DEBUG_PRINTF("[SensorManager] CCS811 não responde em 0x%02X\n", addresses[i]);
            continue;
        }
        
        if (_ccs811.begin(addresses[i])) {
            DEBUG_PRINTLN("[SensorManager] CCS811: Aguardando warmup (20s)...");
            
            uint32_t startTime = millis();
            
            // WARMUP OBRIGATÓRIO 20s
            while (!_ccs811.available() && (millis() - startTime < CCS811_WARMUP_TIME)) {
                delay(500);
                if ((millis() - startTime) % 5000 == 0) {
                    DEBUG_PRINTF("[SensorManager] Warmup: %lus / 20s\n", 
                                (millis() - startTime) / 1000);
                }
            }
            
            if (_ccs811.available()) {
                DEBUG_PRINTLN("[SensorManager] CCS811 disponível!");
                
                // Compensação ambiental com BMP280/SI7021
                if (_bmp280Online || _si7021Online) {
                    float temp = _bmp280Online ? _bmp280.readTemperature() : 25.0;
                    float hum = _si7021Online ? 50.0 : 50.0;  // Ler SI7021 se disponível
                    
                    _ccs811.setEnvironmentalData(hum, temp);
                    DEBUG_PRINTF("[SensorManager] CCS811: Compensação T=%.1f°C H=%.1f%%\n", 
                                temp, hum);
                }
                
                return true;
            } else {
                DEBUG_PRINTLN("[SensorManager] CCS811: Timeout warmup");
            }
        }
    }
    
    DEBUG_PRINTLN("[SensorManager] CCS811: Não inicializado");
    return false;
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

bool SensorManager::_validateBMPReadings(float temperature, float pressure) {
    if (isnan(temperature) || isnan(pressure)) return false;
    if (temperature < TEMP_MIN_VALID || temperature > TEMP_MAX_VALID) return false;
    if (pressure < PRESSURE_MIN_VALID*100 || pressure > PRESSURE_MAX_VALID*100) return false;
    return true;
}

bool SensorManager::_validateSI7021Readings(float temperature, float humidity) {
    if (isnan(temperature) || isnan(humidity)) return false;
    if (temperature < TEMP_MIN_VALID || temperature > TEMP_MAX_VALID) return false;
    if (humidity < HUMIDITY_MIN_VALID || humidity > HUMIDITY_MAX_VALID) return false;
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
    if (!_mpu9250Online) return false;
    
    DEBUG_PRINTLN("[SensorManager] Calibrando MPU9250...");
    
    _mpu9250.autoOffsets();
    delay(100);
    
    DEBUG_PRINTLN("[SensorManager] Calibração concluída!");
    return true;
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
    DEBUG_PRINTLN("[SensorManager] Scanning I2C bus...");
    uint8_t count = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            DEBUG_PRINTF("  Device at 0x%02X\n", addr);
            count++;
        }
    }
    
    DEBUG_PRINTF("[SensorManager] Found %d devices\n", count);
}

void SensorManager::_updateTemperatureRedundancy() {
    // ========================================
    // ESTRATÉGIA DE REDUNDÂNCIA AUTOMÁTICA
    // ========================================
    
    // Prioridade 1: SI7021 (mais preciso para temperatura ambiente)
    if (_si7021Online && _si7021TempValid && !isnan(_temperatureSI)) {
        _temperature = _temperatureSI;
        return;
    }
    
    // Fallback 2: BMP280
    if (_bmp280Online && _bmp280TempValid && !isnan(_temperatureBMP)) {
        _temperature = _temperatureBMP;
        
        // Log apenas quando houver mudança de fonte
        static bool lastUsedBMP = false;
        if (!lastUsedBMP) {
            DEBUG_PRINTLN("[SensorManager] ⚠️  Temperatura: Usando BMP280 (SI7021 indisponível)");
            lastUsedBMP = true;
        }
        return;
    }
    
    // Ambos falharam
    _temperature = NAN;
    
    static unsigned long lastWarning = 0;
    if (millis() - lastWarning > 30000) {
        lastWarning = millis();
        DEBUG_PRINTLN("[SensorManager] ⚠️  CRÍTICO: Ambos sensores de temperatura falharam!");
        DEBUG_PRINTF("[SensorManager]   SI7021: %s (falhas: %d)\n", 
                     _si7021TempValid ? "OK" : "FALHOU", _si7021TempFailures);
        DEBUG_PRINTF("[SensorManager]   BMP280: %s (falhas: %d)\n", 
                     _bmp280TempValid ? "OK" : "FALHOU", _bmp280TempFailures);
    }
}

bool SensorManager::_validateTemperature(float temp) {
    if (isnan(temp)) return false;
    if (temp < TEMP_MIN_VALID || temp > TEMP_MAX_VALID) return false;
    
    // Filtro adicional: Detectar valores absurdos
    if (temp == 0.0 || temp == -273.15) return false;
    
    return true;
}
