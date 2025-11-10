/**
 * @file SensorManager.cpp
 * @brief Gerenciamento de sensores PION
 * @version 3.1.0 - PRODUÇÃO
 * 
 * Sensores operacionais:
 * - MPU9250 (0x68): Accel + Gyro + Mag (9-axis)
 * - BMP280 (0x76): Pressão + Temperatura
 * - SI7021 (0x40): Umidade (temp do BMP280, problema de hw)
 * - CCS811 (0x5A): CO2 + TVOC
 * 
 * Total: 15 parâmetros
 */

#include "SensorManager.h"

SensorManager::SensorManager() :
    _mpu9250(MPU9250_ADDRESS),
    _si7021(),
    _temperature(NAN), _pressure(NAN), _altitude(NAN),
    _humidity(NAN), _co2Level(NAN), _tvoc(NAN),
    _seaLevelPressure(1013.25),
    _gyroX(0.0), _gyroY(0.0), _gyroZ(0.0),
    _accelX(0.0), _accelY(0.0), _accelZ(0.0),
    _magX(0.0), _magY(0.0), _magZ(0.0),
    _mpu9250Online(false), _bmp280Online(false),
    _si7021Online(false), _ccs811Online(false),
    _calibrated(false),
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
        DEBUG_PRINTLN("[SensorManager] MPU9250: ONLINE");
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

    // CCS811 (CO2 + VOC)
    _ccs811Online = _initCCS811();
    if (_ccs811Online) {
        sensorsFound++;  // GARANTIR QUE INCREMENTA
        DEBUG_PRINTLN("[SensorManager] CCS811: ONLINE");  // ADICIONAR DEBUG
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

    _accelX = _applyFilter(gValues.x, _accelXBuffer, _sumAccelX);
    _accelY = _applyFilter(gValues.y, _accelYBuffer, _sumAccelY);
    _accelZ = _applyFilter(gValues.z, _accelZBuffer, _sumAccelZ);
    
    _gyroX = gyrValues.x;
    _gyroY = gyrValues.y;
    _gyroZ = gyrValues.z;
    
    _magX = magValues.x;
    _magY = magValues.y;
    _magZ = magValues.z;
    
    _consecutiveFailures = 0;
}

void SensorManager::_updateBMP280() {
    if (!_bmp280Online) return;

    float temp = _bmp280.readTemperature();
    float press = _bmp280.readPressure();

    if (_validateBMPReadings(temp, press)) {
        _pressure = press / 100.0;
        _altitude = _calculateAltitude(_pressure);
        
        // Usar temperatura do BMP280 apenas se SI7021 falhar
        if (!_si7021Online || isnan(_temperature)) {
            _temperature = temp;
        }
    }
}

void SensorManager::_updateSI7021() {
    if (!_si7021Online) return;

    uint32_t currentTime = millis();
    if (currentTime - _lastSI7021Read < SI7021_READ_INTERVAL) return;
    
    _lastSI7021Read = currentTime;

    // Ler apenas umidade (No Hold Mode)
    Wire.beginTransmission(SI7021_ADDRESS);
    Wire.write(0xF5);
    if (Wire.endTransmission() != 0) return;
    
    delay(30);
    
    Wire.requestFrom((uint8_t)SI7021_ADDRESS, (uint8_t)2);
    if (Wire.available() >= 2) {
        uint16_t rawHum = (Wire.read() << 8) | Wire.read();
        float hum = ((125.0 * rawHum) / 65536.0) - 6.0;
        
        if (hum >= HUMIDITY_MIN_VALID && hum <= HUMIDITY_MAX_VALID) {
            _humidity = hum;
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
    DEBUG_PRINTF("  BMP280:  %s\n", _bmp280Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  SI7021:  %s\n", _si7021Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  CCS811:  %s\n", _ccs811Online ? "ONLINE" : "offline");
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
    
    // ADICIONAR: Inicializar magnetômetro
    if (!_mpu9250.initMagnetometer()) {
        DEBUG_PRINTLN("[SensorManager] Magnetometro falhou, continuando sem ele");
    } else {
        DEBUG_PRINTLN("[SensorManager] Magnetometro OK!");
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
    DEBUG_PRINTLN("[SensorManager] Inicializando SI7021...");
    
    // Reset do sensor
    Wire.beginTransmission(SI7021_ADDRESS);
    Wire.write(0xFE);
    Wire.endTransmission();
    delay(50);
    
    // Testar leitura de umidade (No Hold Mode)
    Wire.beginTransmission(SI7021_ADDRESS);
    Wire.write(0xF5);  // Measure RH, No Hold
    if (Wire.endTransmission() != 0) {
        DEBUG_PRINTLN("[SensorManager] SI7021: Falha ao comunicar");
        return false;
    }
    
    delay(30);
    
    Wire.requestFrom((uint8_t)SI7021_ADDRESS, (uint8_t)2);
    if (Wire.available() >= 2) {
        uint16_t rawHum = (Wire.read() << 8) | Wire.read();
        float humidity = ((125.0 * rawHum) / 65536.0) - 6.0;
        
        if (humidity >= 0.0 && humidity <= 100.0) {
            DEBUG_PRINTF("[SensorManager] SI7021: OK (%.1f%% RH)\n", humidity);
            DEBUG_PRINTLN("[SensorManager] Nota: Temperatura vem do BMP280");
            return true;
        }
    }
    
    DEBUG_PRINTLN("[SensorManager] SI7021: Falha na leitura");
    return false;
}

bool SensorManager::_initCCS811() {
    DEBUG_PRINTLN("[SensorManager] Tentando inicializar CCS811...");
    
    uint8_t addresses[] = {CCS811_ADDR_1, CCS811_ADDR_2};
    
    for (uint8_t i = 0; i < 2; i++) {
        DEBUG_PRINTF("[SensorManager] Testando CCS811 em 0x%02X\n", addresses[i]);
        
        // Verificar presença no barramento I2C
        Wire.beginTransmission(addresses[i]);
        uint8_t error = Wire.endTransmission();
        
        if (error != 0) {
            DEBUG_PRINTF("[SensorManager] CCS811 não responde em 0x%02X (error: %d)\n", addresses[i], error);
            continue;
        }
        
        DEBUG_PRINTF("[SensorManager] CCS811 detectado em 0x%02X, tentando begin()...\n", addresses[i]);
        
        if (_ccs811.begin(addresses[i])) {
            DEBUG_PRINTLN("[SensorManager] CCS811 begin() OK, aguardando disponibilidade...");
            
            uint32_t startTime = millis();
            
            while (!_ccs811.available() && (millis() - startTime < 5000)) {
                delay(100);
            }
            
            if (_ccs811.available()) {
                DEBUG_PRINTLN("[SensorManager] CCS811 disponível!");
                return true;
            } else {
                DEBUG_PRINTLN("[SensorManager] CCS811 timeout ao aguardar disponibilidade");
            }
        } else {
            DEBUG_PRINTF("[SensorManager] CCS811 begin() falhou em 0x%02X\n", addresses[i]);
        }
    }
    
    DEBUG_PRINTLN("[SensorManager] CCS811 não inicializado");
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
        DEBUG_PRINTLN("[SensorManager] Health check: Falhas criticas, resetando...");
        resetAll();
        _consecutiveFailures = 5;
    }
}

bool SensorManager::_calibrateMPU9250() {
    if (!_mpu9250Online) return false;
    
    DEBUG_PRINTLN("[SensorManager] Calibrando MPU9250...");
    
    _mpu9250.autoOffsets();
    delay(100);
    
    DEBUG_PRINTLN("[SensorManager] Calibracao concluida!");
    return true;
}

float SensorManager::_applyFilter(float newValue, float* buffer, float& sum) {
    sum -= buffer[_filterIndex];
    buffer[_filterIndex] = newValue;
    sum += newValue;
    _filterIndex = (_filterIndex + 1) % CUSTOM_FILTER_SIZE;
    return sum / CUSTOM_FILTER_SIZE;
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
