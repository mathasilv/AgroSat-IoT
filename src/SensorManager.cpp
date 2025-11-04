/**
 * @file SensorManager.cpp
 * @brief Implementação com auto-detecção e bibliotecas corrigidas
 * @version 2.1.0
 */

#include "SensorManager.h"

SensorManager::SensorManager() :
    _temperature(NAN), _pressure(NAN), _altitude(NAN),
    _humidity(NAN), _co2Level(NAN), _tvoc(NAN),
    _seaLevelPressure(1013.25),
    _gyroX(0.0), _gyroY(0.0), _gyroZ(0.0),
    _accelX(0.0), _accelY(0.0), _accelZ(0.0),
    _magX(0.0), _magY(0.0), _magZ(0.0),
    _gyroOffsetX(0.0), _gyroOffsetY(0.0), _gyroOffsetZ(0.0),
    _accelOffsetX(0.0), _accelOffsetY(0.0), _accelOffsetZ(0.0),
    _mpu6050Online(false), _mpu9250Online(false),
    _bmp280Online(false), _sht20Online(false), _ccs811Online(false),
    _calibrated(false),
    _lastReadTime(0), _lastCCS811Read(0), _lastSHT20Read(0),
    _lastHealthCheck(0), _consecutiveFailures(0),
    _filterIndex(0)
#ifdef USE_MPU9250
    , _mpu9250(MPU9250_ADDRESS)
#endif
{
    // Inicializar buffers de filtro
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0;
        _accelYBuffer[i] = 0.0;
        _accelZBuffer[i] = 0.0;
    }
}

bool SensorManager::begin() {
    DEBUG_PRINTLN("[SensorManager] Inicializando I2C e detectando sensores...");
    
    // Configurar I2C
    Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    Wire.setClock(I2C_FREQUENCY);
    delay(200); // Estabilização crítica
    
    // Escanear barramento
    scanI2C();
    
    bool success = false;
    uint8_t sensorsFound = 0;
    
    // === SENSORES OBRIGATÓRIOS ===
    
    // IMU: Tentar MPU6050 primeiro, depois MPU9250
    _mpu6050Online = _initMPU6050();
    if (_mpu6050Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] ✓ MPU6050 (IMU 6-DOF) ONLINE");
    } else {
        // Se MPU6050 falhou, tentar MPU9250
#ifdef USE_MPU9250
        _mpu9250Online = _initMPU9250();
        if (_mpu9250Online) {
            sensorsFound++;
            success = true;
            DEBUG_PRINTLN("[SensorManager] ✓ MPU9250 (IMU 9-DOF) ONLINE");
        }
#endif
    }
    
    // BMP280: Obrigatório para OBSAT
    _bmp280Online = _initBMP280();
    if (_bmp280Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] ✓ BMP280 (Pressão/Temp) ONLINE");
    }
    
    // === SENSORES OPCIONAIS ===
    
    // SHT20: Temperatura/Umidade
#ifdef USE_SHT20
    _sht20Online = _initSHT20();
    if (_sht20Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] ✓ SHT20 (Temp/Umidade) ONLINE");
    }
#endif
    
    // CCS811: CO2/TVOC
#ifdef USE_CCS811
    _ccs811Online = _initCCS811();
    if (_ccs811Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] ✓ CCS811 (CO2/TVOC) ONLINE");
    }
#endif
    
    // Calibração do IMU disponível
    if (_mpu6050Online || _mpu9250Online) {
        DEBUG_PRINTLN("[SensorManager] Calibrando IMU...");
        calibrateIMU();
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========== RESUMO DOS SENSORES ==========");
    DEBUG_PRINTF("Total detectado: %d sensores\n", sensorsFound);
    printSensorStatus();
    DEBUG_PRINTF("Heap após init: %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
    
    return success; // Sucesso se pelo menos IMU ou BMP280 funcionar
}

void SensorManager::update() {
    uint32_t currentTime = millis();
    
    // Health check periódico
    if (currentTime - _lastHealthCheck >= 30000) {
        _lastHealthCheck = currentTime;
        _performHealthCheck();
    }
    
    // Leitura dos sensores principais
    if (currentTime - _lastReadTime >= SENSOR_READ_INTERVAL) {
        _lastReadTime = currentTime;
        
        // MPU6050 (IMU 6-DOF)
        if (_mpu6050Online) {
            sensors_event_t accel, gyro, temp;
            if (_mpu6050.getEvent(&accel, &gyro, &temp)) {
                if (_validateMPUReadings(accel, gyro)) {
                    // Aplicar offsets de calibração
                    _gyroX = gyro.gyro.x - _gyroOffsetX;
                    _gyroY = gyro.gyro.y - _gyroOffsetY;
                    _gyroZ = gyro.gyro.z - _gyroOffsetZ;
                    
                    // Aplicar filtro
                    _accelX = _applyFilter(accel.acceleration.x - _accelOffsetX, _accelXBuffer);
                    _accelY = _applyFilter(accel.acceleration.y - _accelOffsetY, _accelYBuffer);
                    _accelZ = _applyFilter(accel.acceleration.z - _accelOffsetZ, _accelZBuffer);
                    
                    _consecutiveFailures = 0;
                } else {
                    _consecutiveFailures++;
                }
            } else {
                _consecutiveFailures++;
            }
        }
        
        // MPU9250 (IMU 9-DOF)
#ifdef USE_MPU9250
        if (_mpu9250Online && !_mpu6050Online) {  // Usar só se 6050 não disponível
            xyzFloat gValues = _mpu9250.getGValues();
            xyzFloat gyrValues = _mpu9250.getGyrValues();
            xyzFloat magValues = _mpu9250.getMagValues();
            
            // IMU 6-DOF
            _accelX = _applyFilter(gValues.x - _accelOffsetX, _accelXBuffer);
            _accelY = _applyFilter(gValues.y - _accelOffsetY, _accelYBuffer);
            _accelZ = _applyFilter(gValues.z - _accelOffsetZ, _accelZBuffer);
            
            _gyroX = gyrValues.x - _gyroOffsetX;
            _gyroY = gyrValues.y - _gyroOffsetY;
            _gyroZ = gyrValues.z - _gyroOffsetZ;
            
            // Magnetômetro (9º DOF)
            _magX = magValues.x;
            _magY = magValues.y;
            _magZ = magValues.z;
        }
#endif
        
        // BMP280 (Pressão/Temperatura)
        if (_bmp280Online) {
            float temp = _bmp280.readTemperature();
            float press = _bmp280.readPressure();
            
            if (_validateBMPReadings(temp, press)) {
                _temperature = temp;
                _pressure = press / 100.0; // Pa -> hPa
                _altitude = _calculateAltitude(_pressure);
            }
        }
    }
    
    // SHT20 (leitura mais lenta)
#ifdef USE_SHT20
    if (_sht20Online && (currentTime - _lastSHT20Read >= SHT20_READ_INTERVAL)) {
        _lastSHT20Read = currentTime;
        
        float temp = _sht20.getTemperature();
        float hum = _sht20.getHumidity();
        
        if (_validateSHTReadings(temp, hum)) {
            // Usar temperatura do SHT20 se BMP280 não disponível
            if (!_bmp280Online) {
                _temperature = temp;
            }
            _humidity = hum;
        }
    }
#endif
    
    // CCS811 (leitura ainda mais lenta)
#ifdef USE_CCS811
    if (_ccs811Online && (currentTime - _lastCCS811Read >= CCS811_READ_INTERVAL)) {
        _lastCCS811Read = currentTime;
        
        if (_ccs811.available()) {
            if (!_ccs811.readData()) {
                float co2 = _ccs811.geteCO2();
                float tvoc = _ccs811.getTVOC();
                
                if (_validateCCSReadings(co2, tvoc)) {
                    _co2Level = co2;
                    _tvoc = tvoc;
                }
            }
        }
    }
#endif
}

// ============================================================================
// GETTERS (COMPATIBILIDADE TOTAL)
// ============================================================================

float SensorManager::getTemperature() { return _temperature; }
float SensorManager::getPressure() { return _pressure; }
float SensorManager::getAltitude() { return _altitude; }
float SensorManager::getGyroX() { return _gyroX; }
float SensorManager::getGyroY() { return _gyroY; }
float SensorManager::getGyroZ() { return _gyroZ; }
float SensorManager::getAccelX() { return _accelX; }
float SensorManager::getAccelY() { return _accelY; }
float SensorManager::getAccelZ() { return _accelZ; }

float SensorManager::getAccelMagnitude() {
    return sqrt(_accelX * _accelX + _accelY * _accelY + _accelZ * _accelZ);
}

// Getters expandidos
float SensorManager::getHumidity() { return _humidity; }
float SensorManager::getCO2() { return _co2Level; }
float SensorManager::getTVOC() { return _tvoc; }
float SensorManager::getMagX() { return _magX; }
float SensorManager::getMagY() { return _magY; }
float SensorManager::getMagZ() { return _magZ; }

// Status
bool SensorManager::isMPU6050Online() { return _mpu6050Online; }
bool SensorManager::isMPU9250Online() { return _mpu9250Online; }
bool SensorManager::isBMP280Online() { return _bmp280Online; }
bool SensorManager::isSHT20Online() { return _sht20Online; }
bool SensorManager::isCCS811Online() { return _ccs811Online; }
bool SensorManager::isCalibrated() { return _calibrated; }

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("[SensorManager] Escaneando I2C...");
    uint8_t found = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            DEBUG_PRINTF("  Dispositivo em 0x%02X\n", address);
            found++;
        }
    }
    
    DEBUG_PRINTF("[SensorManager] %d dispositivos encontrados\n", found);
}

void SensorManager::printSensorStatus() {
    DEBUG_PRINTLN("Status dos sensores:");
    DEBUG_PRINTF("  MPU6050: %s\n", _mpu6050Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  MPU9250: %s\n", _mpu9250Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  BMP280:  %s\n", _bmp280Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  SHT20:   %s\n", _sht20Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  CCS811:  %s\n", _ccs811Online ? "ONLINE" : "offline");
}

bool SensorManager::calibrateIMU() {
    if (_mpu6050Online) {
        return _calibrateMPU6050();
    }
    
#ifdef USE_MPU9250
    if (_mpu9250Online) {
        DEBUG_PRINTLN("[SensorManager] Calibrando MPU9250...");
        _mpu9250.autoOffsets();
        _calibrated = true;
        return true;
    }
#endif
    
    return false;
}

void SensorManager::resetMPU6050() {
    if (_mpu6050Online) {
        _mpu6050Online = _initMPU6050();
    }
}

void SensorManager::resetBMP280() {
    if (_bmp280Online) {
        _bmp280Online = _initBMP280();
    }
}

void SensorManager::resetAll() {
    DEBUG_PRINTLN("[SensorManager] Reiniciando todos os sensores...");
    
    _mpu6050Online = _initMPU6050();
    _bmp280Online = _initBMP280();
    
#ifdef USE_MPU9250
    if (!_mpu6050Online) {
        _mpu9250Online = _initMPU9250();
    }
#endif

#ifdef USE_SHT20
    _sht20Online = _initSHT20();
#endif

#ifdef USE_CCS811
    _ccs811Online = _initCCS811();
#endif
    
    _consecutiveFailures = 0;
}

void SensorManager::getRawData(sensors_event_t& accel, sensors_event_t& gyro, sensors_event_t& temp) {
    if (_mpu6050Online) {
        _mpu6050.getEvent(&accel, &gyro, &temp);
    }
}

// ============================================================================
// MÉTODOS PRIVADOS - INICIALIZAÇÃO DE CADA SENSOR
// ============================================================================

bool SensorManager::_initMPU6050() {
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            delay(100);
            DEBUG_PRINTF("[SensorManager] Retry MPU6050 %d/3\n", attempt + 1);
        }
        
        if (_mpu6050.begin(MPU6050_ADDRESS, &Wire)) {
            // Configurar ranges
            _mpu6050.setAccelerometerRange(MPU6050_RANGE_8_G);
            _mpu6050.setGyroRange(MPU6050_RANGE_500_DEG);
            _mpu6050.setFilterBandwidth(MPU6050_BAND_21_HZ);
            
            // Teste de leitura
            delay(50);
            sensors_event_t accel, gyro, temp;
            if (_mpu6050.getEvent(&accel, &gyro, &temp)) {
                return true;
            }
        }
    }
    return false;
}

bool SensorManager::_initBMP280() {
    uint8_t addresses[] = {BMP280_ADDR_1, BMP280_ADDR_2};
    
    for (uint8_t i = 0; i < 2; i++) {
        if (_bmp280.begin(addresses[i])) {
            // Configurar para máxima precisão
            _bmp280.setSampling(Adafruit_BMP280::MODE_NORMAL,
                              Adafruit_BMP280::SAMPLING_X16,
                              Adafruit_BMP280::SAMPLING_X16,
                              Adafruit_BMP280::FILTER_X16,
                              Adafruit_BMP280::STANDBY_MS_500);
            
            // Teste de leitura
            delay(100);
            float testTemp = _bmp280.readTemperature();
            if (!isnan(testTemp) && testTemp > TEMP_MIN_VALID && testTemp < TEMP_MAX_VALID) {
                DEBUG_PRINTF("[SensorManager] BMP280 OK em 0x%02X\n", addresses[i]);
                return true;
            }
        }
    }
    return false;
}

#ifdef USE_MPU9250
bool SensorManager::_initMPU9250() {
    if (_mpu9250.init()) {
        // Configurar ranges
        _mpu9250.setAccRange(MPU9250_ACC_RANGE_8G);
        _mpu9250.setGyrRange(MPU9250_GYRO_RANGE_500);
        _mpu9250.enableGyrDLPF();
        _mpu9250.setGyrDLPF(MPU9250_DLPF_6);
        
        // Teste de leitura
        delay(100);
        xyzFloat gValues = _mpu9250.getGValues();
        if (!isnan(gValues.x)) {
            return true;
        }
    }
    return false;
}
#endif

#ifdef USE_SHT20
bool SensorManager::_initSHT20() {
    // Verificar se SHT20 está presente
    Wire.beginTransmission(SHT20_ADDRESS);
    if (Wire.endTransmission() == 0) {
        _sht20.begin();
        
        // Aguardar estabilização
        delay(500);
        
        // Teste de leitura
        float testTemp = _sht20.getTemperature();
        float testHum = _sht20.getHumidity();
        
        if (_validateSHTReadings(testTemp, testHum)) {
            return true;
        }
    }
    return false;
}
#endif

#ifdef USE_CCS811
bool SensorManager::_initCCS811() {
    uint8_t addresses[] = {CCS811_ADDR_1, CCS811_ADDR_2};
    
    for (uint8_t i = 0; i < 2; i++) {
        if (_ccs811.begin(addresses[i])) {
            // Aguardar sensor ficar disponível
            uint32_t startTime = millis();
            while (!_ccs811.available() && (millis() - startTime < 3000)) {
                delay(100);
            }
            
            if (_ccs811.available()) {
                DEBUG_PRINTF("[SensorManager] CCS811 OK em 0x%02X\n", addresses[i]);
                return true;
            }
        }
    }
    return false;
}
#endif

// ============================================================================
// MÉTODOS DE VALIDAÇÃO
// ============================================================================

bool SensorManager::_validateMPUReadings(const sensors_event_t& accel, const sensors_event_t& gyro) {
    // Verificar NaN
    if (isnan(accel.acceleration.x) || isnan(accel.acceleration.y) || isnan(accel.acceleration.z) ||
        isnan(gyro.gyro.x) || isnan(gyro.gyro.y) || isnan(gyro.gyro.z)) {
        return false;
    }
    
    // Verificar ranges razoáveis (±8G = ±78.4 m/s², ±500°/s = ±8.7 rad/s)
    if (abs(accel.acceleration.x) > 80 || abs(accel.acceleration.y) > 80 || abs(accel.acceleration.z) > 80) {
        return false;
    }
    
    if (abs(gyro.gyro.x) > 10 || abs(gyro.gyro.y) > 10 || abs(gyro.gyro.z) > 10) {
        return false;
    }
    
    return true;
}

bool SensorManager::_validateBMPReadings(float temperature, float pressure) {
    if (isnan(temperature) || isnan(pressure)) return false;
    
    if (temperature < TEMP_MIN_VALID || temperature > TEMP_MAX_VALID) return false;
    
    float pressureHPa = pressure / 100.0;
    if (pressureHPa < PRESSURE_MIN_VALID || pressureHPa > PRESSURE_MAX_VALID) return false;
    
    return true;
}

bool SensorManager::_validateSHTReadings(float temperature, float humidity) {
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

// ============================================================================
// MÉTODOS AUXILIARES
// ============================================================================

void SensorManager::_performHealthCheck() {
    DEBUG_PRINTF("[SensorManager] Health - Sensores: %d online, Falhas: %d\n",
                (_mpu6050Online + _mpu9250Online + _bmp280Online + _sht20Online + _ccs811Online),
                _consecutiveFailures);
                
    if (_consecutiveFailures >= 10) {
        DEBUG_PRINTLN("[SensorManager] Muitas falhas - reset automático");
        resetAll();
        _consecutiveFailures = 5;
    }
}

bool SensorManager::_calibrateMPU6050() {
    if (!_mpu6050Online) return false;
    
    DEBUG_PRINTLN("[SensorManager] Calibrando MPU6050 (mantenha imóvel)...");
    
    float sumGyroX = 0, sumGyroY = 0, sumGyroZ = 0;
    float sumAccelX = 0, sumAccelY = 0, sumAccelZ = 0;
    uint16_t validSamples = 0;
    
    for (int i = 0; i < MPU6050_CALIBRATION_SAMPLES; i++) {
        sensors_event_t accel, gyro, temp;
        
        if (_mpu6050.getEvent(&accel, &gyro, &temp) && 
            _validateMPUReadings(accel, gyro)) {
            
            sumGyroX += gyro.gyro.x;
            sumGyroY += gyro.gyro.y;
            sumGyroZ += gyro.gyro.z;
            
            sumAccelX += accel.acceleration.x;
            sumAccelY += accel.acceleration.y;
            sumAccelZ += accel.acceleration.z;
            
            validSamples++;
        }
        
        delay(10);
        if (i % 20 == 0) DEBUG_PRINT(".");
    }
    DEBUG_PRINTLN("");
    
    if (validSamples < (MPU6050_CALIBRATION_SAMPLES * 0.8)) {
        DEBUG_PRINTF("[SensorManager] Calibração falhou: %d/%d amostras\n", 
                    validSamples, MPU6050_CALIBRATION_SAMPLES);
        return false;
    }
    
    // Calcular offsets
    _gyroOffsetX = sumGyroX / validSamples;
    _gyroOffsetY = sumGyroY / validSamples;
    _gyroOffsetZ = sumGyroZ / validSamples;
    
    _accelOffsetX = sumAccelX / validSamples;
    _accelOffsetY = sumAccelY / validSamples;
    _accelOffsetZ = (sumAccelZ / validSamples) - 9.81; // Subtrair gravidade
    
    DEBUG_PRINTF("[SensorManager] Calibração OK (%d amostras)\n", validSamples);
    DEBUG_PRINTF("  Gyro offsets: [%.4f, %.4f, %.4f]\n", _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ);
    DEBUG_PRINTF("  Accel offsets: [%.4f, %.4f, %.4f]\n", _accelOffsetX, _accelOffsetY, _accelOffsetZ);
    
    return true;
}

float SensorManager::_applyFilter(float newValue, float* buffer) {
    buffer[_filterIndex] = newValue;
    _filterIndex = (_filterIndex + 1) % CUSTOM_FILTER_SIZE;
    
    float sum = 0.0;
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        sum += buffer[i];
    }
    
    return sum / CUSTOM_FILTER_SIZE;
}

float SensorManager::_calculateAltitude(float pressure) {
    if (pressure <= 0.0) return 0.0;
    
    float ratio = pressure / _seaLevelPressure;
    return 44330.0 * (1.0 - pow(ratio, 0.1903));
}
