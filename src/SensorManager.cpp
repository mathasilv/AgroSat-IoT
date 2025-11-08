/**
 * @file SensorManager.cpp (refatorado)
 * @brief Versão otimizada: sem redundâncias, I2C centralizado, logs compactos, filtro O(1) incremental
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
    _mpu6050Online(false), _mpu9250Online(false),
    _bmp280Online(false), _sht20Online(false), _ccs811Online(false),
    _calibrated(false),
    _lastReadTime(0), _lastCCS811Read(0), _lastSHT20Read(0),
    _lastHealthCheck(0), _consecutiveFailures(0), _filterIndex(0),
    _sumAccelX(0), _sumAccelY(0), _sumAccelZ(0)   // Passar a somas para membros da classe
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
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores...");
    bool success = false;
    uint8_t sensorsFound = 0;

    _mpu6050Online = _initMPU6050();
    if (_mpu6050Online) { sensorsFound++; success = true; }
#ifdef USE_MPU9250
    if (!_mpu6050Online) {
        _mpu9250Online = _initMPU9250();
        if (_mpu9250Online) { sensorsFound++; success = true; }
    }
#endif
    _bmp280Online = _initBMP280();
    if (_bmp280Online) { sensorsFound++; success = true; }
#ifdef USE_SHT20
    _sht20Online = _initSHT20();
    if (_sht20Online) sensorsFound++;
#endif
#ifdef USE_CCS811
    _ccs811Online = _initCCS811();
    if (_ccs811Online) sensorsFound++;
#endif

    if (_mpu6050Online || _mpu9250Online) {
        _calibrated = true;
    }
    DEBUG_PRINTF("[SensorManager] %d sensores detectados\n", sensorsFound);
    return success;
}

void SensorManager::update() {
    uint32_t currentTime = millis();

    if (currentTime - _lastHealthCheck >= 30000) {
        _lastHealthCheck = currentTime;
#ifdef DEBUG_VERBOSE
        _performHealthCheck();
#endif
    }
    if (currentTime - _lastReadTime >= SENSOR_READ_INTERVAL) {
        _lastReadTime = currentTime;
        _updateIMU();
        _updateBMP280();
        _updateSHT20();
        _updateCCS811();
    }
}

// Funções auxiliares modulares para atualização de sensores
void SensorManager::_updateIMU() {
    if (_mpu6050Online) {
        _mpu6050.update();
        _gyroX = _mpu6050.getGyroX(); _gyroY = _mpu6050.getGyroY(); _gyroZ = _mpu6050.getGyroZ();
        float rawAccelX = _mpu6050.getAccX(); float rawAccelY = _mpu6050.getAccY(); float rawAccelZ = _mpu6050.getAccZ();
        if (!(isnan(_gyroX)||isnan(_gyroY)||isnan(_gyroZ)||isnan(rawAccelX)||isnan(rawAccelY)||isnan(rawAccelZ))) {
            _accelX = _applyFilter(rawAccelX, _accelXBuffer, _sumAccelX);
            _accelY = _applyFilter(rawAccelY, _accelYBuffer, _sumAccelY);
            _accelZ = _applyFilter(rawAccelZ, _accelZBuffer, _sumAccelZ);
            _consecutiveFailures = 0;
        } else {
            _consecutiveFailures++;
        }
    }
#ifdef USE_MPU9250
    else if (_mpu9250Online) {
        xyzFloat gValues = _mpu9250.getGValues();
        xyzFloat gyrValues = _mpu9250.getGyrValues();
        xyzFloat magValues = _mpu9250.getMagValues();
        _accelX = _applyFilter(gValues.x, _accelXBuffer, _sumAccelX);
        _accelY = _applyFilter(gValues.y, _accelYBuffer, _sumAccelY);
        _accelZ = _applyFilter(gValues.z, _accelZBuffer, _sumAccelZ);
        _gyroX = gyrValues.x; _gyroY = gyrValues.y; _gyroZ = gyrValues.z;
        _magX = magValues.x; _magY = magValues.y; _magZ = magValues.z;
    }
#endif
}

void SensorManager::_updateBMP280() {
    if (_bmp280Online) {
        float temp = _bmp280.readTemperature();
        float press = _bmp280.readPressure();
        if (!(isnan(temp)||isnan(press))) {
            _temperature = temp;
            _pressure = press / 100.0;
            _altitude = _calculateAltitude(_pressure);
        }
    }
}

void SensorManager::_updateSHT20() {
#ifdef USE_SHT20
    uint32_t currentTime = millis();
    if (_sht20Online && (currentTime - _lastSHT20Read >= SHT20_READ_INTERVAL)) {
        _lastSHT20Read = currentTime;
        float temp = _sht20.getTemperature();
        float hum = _sht20.getHumidity();
        if (!(isnan(temp)||isnan(hum))) {
            if (!_bmp280Online) _temperature = temp;
            _humidity = hum;
        }
    }
#endif
}

void SensorManager::_updateCCS811() {
#ifdef USE_CCS811
    uint32_t currentTime = millis();
    if (_ccs811Online && (currentTime - _lastCCS811Read >= CCS811_READ_INTERVAL)) {
        _lastCCS811Read = currentTime;
        if (_ccs811.available() && !_ccs811.readData()) {
            float co2 = _ccs811.geteCO2();
            float tvoc = _ccs811.getTVOC();
            if (!(isnan(co2) || isnan(tvoc))) {
                _co2Level = co2;
                _tvoc = tvoc;
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
    return sqrt(_accelX*_accelX + _accelY*_accelY + _accelZ*_accelZ); 
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

void SensorManager::printSensorStatus() {
    DEBUG_PRINTF("  MPU6050: %s\n", _mpu6050Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  MPU9250: %s\n", _mpu9250Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  BMP280:  %s\n", _bmp280Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  SHT20:   %s\n", _sht20Online ? "ONLINE" : "offline");
    DEBUG_PRINTF("  CCS811:  %s\n", _ccs811Online ? "ONLINE" : "offline");
}

void SensorManager::resetMPU6050() { 
    if (_mpu6050Online) _mpu6050Online = _initMPU6050(); 
}

void SensorManager::resetBMP280() { 
    if (_bmp280Online) _bmp280Online = _initBMP280(); 
}

void SensorManager::resetAll() {
    _mpu6050Online = _initMPU6050(); 
    _bmp280Online = _initBMP280();
#ifdef USE_MPU9250
    if (!_mpu6050Online) _mpu9250Online = _initMPU9250();
#endif
#ifdef USE_SHT20
    _sht20Online = _initSHT20();
#endif
#ifdef USE_CCS811
    _ccs811Online = _initCCS811();
#endif
    _consecutiveFailures = 0;
}


bool SensorManager::_initMPU6050() {
    Wire.beginTransmission(MPU6050_ADDRESS);
    if (Wire.endTransmission() != 0) return false;

    Wire.beginTransmission(MPU6050_ADDRESS); Wire.write(0x6B); Wire.write(0x80); Wire.endTransmission(); delay(100);
    Wire.beginTransmission(MPU6050_ADDRESS); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(); delay(50);
    Wire.beginTransmission(MPU6050_ADDRESS); Wire.write(0x75); Wire.endTransmission(false);
    uint8_t bytesReceived = Wire.requestFrom((uint8_t)MPU6050_ADDRESS, (uint8_t)1);
    if (bytesReceived == 1) {
        uint8_t whoami = Wire.read();
        if (!(whoami == 0x68 || whoami == 0x70 || whoami == 0x71 || whoami == 0x73 || whoami == 0x98)) return false;
    } else return false;

    if (_mpu6050.begin() == 0) {
        _mpu6050.calcOffsets(true, true);
        delay(100);
        _mpu6050.update();
        return true;
    }
    return false;
}

bool SensorManager::_initBMP280() {
    uint8_t addresses[] = {BMP280_ADDR_1, BMP280_ADDR_2};
    for (uint8_t i = 0; i < 2; i++) {
        if (_bmp280.begin(addresses[i])) {
            _bmp280.setSampling(Adafruit_BMP280::MODE_NORMAL, Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::SAMPLING_X16, Adafruit_BMP280::FILTER_X16, Adafruit_BMP280::STANDBY_MS_500);
            delay(100);
            float testTemp = _bmp280.readTemperature();
            if (!isnan(testTemp) && testTemp > TEMP_MIN_VALID && testTemp < TEMP_MAX_VALID) return true;
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
        if (!isnan(gValues.x)) return true;
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
        if (!(isnan(testTemp) || isnan(testHum))) return true;
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
            if (_ccs811.available()) return true;
        }
    }
    return false;
}
#endif

void SensorManager::_performHealthCheck() {
#ifdef DEBUG_VERBOSE
    DEBUG_PRINTF("[SensorManager] Health - Falhas: %d\n", _consecutiveFailures);
#endif
    if (_consecutiveFailures >= 10) {
        resetAll();
        _consecutiveFailures = 5;
    }
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
