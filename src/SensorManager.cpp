/**
 * @file SensorManager.cpp
 * @brief Implementação com MPU6050_light e suporte MPU6500/MPU6880 - VERSÃO 2.2.1
 * @version 2.2.1
 */

#include "SensorManager.h"

SensorManager::SensorManager() :
    _mpu6050(Wire),
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
    for (uint8_t i = 0; i < CUSTOM_FILTER_SIZE; i++) {
        _accelXBuffer[i] = 0.0;
        _accelYBuffer[i] = 0.0;
        _accelZBuffer[i] = 0.0;
    }
}

bool SensorManager::begin() {
    static bool i2cInitialized = false;
    
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores...");
    
    if (!i2cInitialized) {
        DEBUG_PRINTLN("[SensorManager] Inicializando I2C pela primeira vez...");
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(I2C_FREQUENCY);
        delay(200);
        i2cInitialized = true;
    } else {
        DEBUG_PRINTLN("[SensorManager] I2C já inicializado (reutilizando)");
    }
    
    scanI2C();
    
    bool success = false;
    uint8_t sensorsFound = 0;
    
    _mpu6050Online = _initMPU6050();
    if (_mpu6050Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] ✓ MPU6050 (IMU 6-DOF) ONLINE");
    } else {
#ifdef USE_MPU9250
        _mpu9250Online = _initMPU9250();
        if (_mpu9250Online) {
            sensorsFound++;
            success = true;
            DEBUG_PRINTLN("[SensorManager] ✓ MPU9250 (IMU 9-DOF) ONLINE");
        }
#endif
    }
    
    _bmp280Online = _initBMP280();
    if (_bmp280Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] ✓ BMP280 (Pressão/Temp) ONLINE");
    }
    
#ifdef USE_SHT20
    _sht20Online = _initSHT20();
    if (_sht20Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] ✓ SHT20 (Temp/Umidade) ONLINE");
    }
#endif
    
#ifdef USE_CCS811
    _ccs811Online = _initCCS811();
    if (_ccs811Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] ✓ CCS811 (CO2/TVOC) ONLINE");
    }
#endif
    
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
        
        if (_mpu6050Online) {
            _mpu6050.update();
            
            _gyroX = _mpu6050.getGyroX();
            _gyroY = _mpu6050.getGyroY();
            _gyroZ = _mpu6050.getGyroZ();
            
            float rawAccelX = _mpu6050.getAccX();
            float rawAccelY = _mpu6050.getAccY();
            float rawAccelZ = _mpu6050.getAccZ();
            
            if (_validateMPUReadings(_gyroX, _gyroY, _gyroZ, 
                                     rawAccelX, rawAccelY, rawAccelZ)) {
                _accelX = _applyFilter(rawAccelX, _accelXBuffer);
                _accelY = _applyFilter(rawAccelY, _accelYBuffer);
                _accelZ = _applyFilter(rawAccelZ, _accelZBuffer);
                
                _consecutiveFailures = 0;
            } else {
                _consecutiveFailures++;
            }
        }
        
#ifdef USE_MPU9250
        if (_mpu9250Online && !_mpu6050Online) {
            xyzFloat gValues = _mpu9250.getGValues();
            xyzFloat gyrValues = _mpu9250.getGyrValues();
            xyzFloat magValues = _mpu9250.getMagValues();
            
            _accelX = _applyFilter(gValues.x - _accelOffsetX, _accelXBuffer);
            _accelY = _applyFilter(gValues.y - _accelOffsetY, _accelYBuffer);
            _accelZ = _applyFilter(gValues.z - _accelOffsetZ, _accelZBuffer);
            
            _gyroX = gyrValues.x - _gyroOffsetX;
            _gyroY = gyrValues.y - _gyroOffsetY;
            _gyroZ = gyrValues.z - _gyroOffsetZ;
            
            _magX = magValues.x;
            _magY = magValues.y;
            _magZ = magValues.z;
        }
#endif
        
        if (_bmp280Online) {
            float temp = _bmp280.readTemperature();
            float press = _bmp280.readPressure();
            
            if (_validateBMPReadings(temp, press)) {
                _temperature = temp;
                _pressure = press / 100.0;
                _altitude = _calculateAltitude(_pressure);
            }
        }
    }
    
#ifdef USE_SHT20
    if (_sht20Online && (currentTime - _lastSHT20Read >= SHT20_READ_INTERVAL)) {
        _lastSHT20Read = currentTime;
        
        float temp = _sht20.getTemperature();
        float hum = _sht20.getHumidity();
        
        if (_validateSHTReadings(temp, hum)) {
            if (!_bmp280Online) {
                _temperature = temp;
            }
            _humidity = hum;
        }
    }
#endif
    
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

float SensorManager::getHumidity() { return _humidity; }
float SensorManager::getCO2() { return _co2Level; }
float SensorManager::getTVOC() { return _tvoc; }
float SensorManager::getMagX() { return _magX; }
float SensorManager::getMagY() { return _magY; }
float SensorManager::getMagZ() { return _magZ; }

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

void SensorManager::getRawData(float& gx, float& gy, float& gz, 
                               float& ax, float& ay, float& az) {
    if (_mpu6050Online) {
        gx = _gyroX;
        gy = _gyroY;
        gz = _gyroZ;
        ax = _accelX;
        ay = _accelY;
        az = _accelZ;
    } else {
        gx = gy = gz = ax = ay = az = NAN;
    }
}

// ============================================================================
// MÉTODOS PRIVADOS - INICIALIZAÇÃO
// ============================================================================

bool SensorManager::_initMPU6050() {
    DEBUG_PRINTLN("[SensorManager] === TESTE DIRETO MPU6050/MPU6500 ===");
    
    Wire.beginTransmission(MPU6050_ADDRESS);
    uint8_t error = Wire.endTransmission();
    DEBUG_PRINTF("  Endereço 0x68: %s (erro=%d)\n", error == 0 ? "ACK" : "NACK", error);
    
    if (error != 0) {
        DEBUG_PRINTLN("  FALHA: Sensor não responde");
        return false;
    }
    
    // Reset via PWR_MGMT_1
    Wire.beginTransmission(MPU6050_ADDRESS);
    Wire.write(0x6B);
    Wire.write(0x80);
    Wire.endTransmission();
    delay(100);
    
    // Wake up
    Wire.beginTransmission(MPU6050_ADDRESS);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission();
    delay(50);
    
    // Ler WHO_AM_I
    Wire.beginTransmission(MPU6050_ADDRESS);
    Wire.write(0x75);
    Wire.endTransmission(false);
    
    uint8_t bytesReceived = Wire.requestFrom((uint8_t)MPU6050_ADDRESS, (uint8_t)1);
    DEBUG_PRINTF("  Bytes recebidos: %d\n", bytesReceived);
    
    if (bytesReceived == 1) {
        uint8_t whoami = Wire.read();
        DEBUG_PRINTF("  WHO_AM_I = 0x%02X ", whoami);
        
        // ACEITAR CLONES MPU6500/MPU6880 (0x71) e MPU9250 (0x73)
        if (whoami == 0x68) {
            DEBUG_PRINTLN("(MPU6050 genuíno)");
        } else if (whoami == 0x70) {
            DEBUG_PRINTLN("(MPU6500)");
        } else if (whoami == 0x71) {
            DEBUG_PRINTLN("(MPU6500/MPU6880 clone)");
        } else if (whoami == 0x73) {
            DEBUG_PRINTLN("(MPU9250)");
        } else if (whoami == 0x98) {
            DEBUG_PRINTLN("(MPU6050 variante)");
        } else {
            DEBUG_PRINTF("(DESCONHECIDO)\n");
            DEBUG_PRINTLN("  FALHA: WHO_AM_I inválido");
            return false;
        }
    } else {
        DEBUG_PRINTLN("  FALHA: Não recebeu dados do WHO_AM_I");
        return false;
    }
    
    DEBUG_PRINTLN("  ✓ Comunicação I2C OK");
    DEBUG_PRINTLN("[SensorManager] === Tentando MPU6050_light.begin() ===");
    
    byte status = _mpu6050.begin();
    DEBUG_PRINTF("  begin() retornou: %d (0=OK)\n", status);
    
    if (status == 0) {
        DEBUG_PRINTLN("  Calculando offsets (mantenha imóvel 3s)...");
        _mpu6050.calcOffsets(true, true);
        
        delay(100);
        _mpu6050.update();
        
        DEBUG_PRINTLN("  ✓ MPU6050_light OK");
        return true;
    } else {
        DEBUG_PRINTF("  FALHA: begin() erro=%d\n", status);
        return false;
    }
}

bool SensorManager::_initBMP280() {
    uint8_t addresses[] = {BMP280_ADDR_1, BMP280_ADDR_2};
    
    for (uint8_t i = 0; i < 2; i++) {
        if (_bmp280.begin(addresses[i])) {
            _bmp280.setSampling(Adafruit_BMP280::MODE_NORMAL,
                              Adafruit_BMP280::SAMPLING_X16,
                              Adafruit_BMP280::SAMPLING_X16,
                              Adafruit_BMP280::FILTER_X16,
                              Adafruit_BMP280::STANDBY_MS_500);
            
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
        _mpu9250.setAccRange(MPU9250_ACC_RANGE_8G);
        _mpu9250.setGyrRange(MPU9250_GYRO_RANGE_500);
        _mpu9250.enableGyrDLPF();
        _mpu9250.setGyrDLPF(MPU9250_DLPF_6);
        
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
    Wire.beginTransmission(SHT20_ADDRESS);
    if (Wire.endTransmission() == 0) {
        _sht20.begin();
        delay(500);
        
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
// VALIDAÇÃO
// ============================================================================

bool SensorManager::_validateMPUReadings(float gx, float gy, float gz,
                                        float ax, float ay, float az) {
    if (isnan(ax) || isnan(ay) || isnan(az) ||
        isnan(gx) || isnan(gy) || isnan(gz)) {
        return false;
    }
    
    if (abs(ax) > 10 || abs(ay) > 10 || abs(az) > 10) {
        return false;
    }
    
    if (abs(gx) > 600 || abs(gy) > 600 || abs(gz) > 600) {
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
// AUXILIARES
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
    
    DEBUG_PRINTLN("[SensorManager] MPU6050_light já calibrado automaticamente");
    _calibrated = true;
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
