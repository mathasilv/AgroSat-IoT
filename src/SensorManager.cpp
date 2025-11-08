/**
 * @file SensorManager.cpp
 * @brief Versão Clean: Só o essencial para missão OBSAT
 * @version 2.3.0
 * @date 2025-11-07
 *
 * CHANGELOG v2.3.0:
 * - Remove scanI2C, printSensorStatus, reset individuais
 * - Saúde checada inline e logging via LOG_PREFLIGHT/LOG_FLIGHT
 * - Calibração IMU é automática; flag/calibrateIMU retirados
 * - Inicialização I2C assume barramento já configurado pelo TelemetryManager
 * - Getter isCalibrated removido
 * - Debug detalhado só no modo verbose (PREFLIGHT)
 */

#include "SensorManager.h"
#include "mission_config.h"

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
    LOG_PREFLIGHT("[SensorManager] Inicializando sensores...\n");
    bool success = false;
    uint8_t sensorsFound = 0;
    _mpu6050Online = _initMPU6050();
#ifdef USE_MPU9250
    if (!_mpu6050Online) {
        _mpu9250Online = _initMPU9250();
        if (_mpu9250Online) sensorsFound++;
    }
#endif
    if (_mpu6050Online || _mpu9250Online) sensorsFound++;
    _bmp280Online = _initBMP280();
    if (_bmp280Online) sensorsFound++;
#ifdef USE_SHT20
    _sht20Online = _initSHT20();
    if (_sht20Online) sensorsFound++;
#endif
#ifdef USE_CCS811
    _ccs811Online = _initCCS811();
    if (_ccs811Online) sensorsFound++;
#endif
    success = (_mpu6050Online || _mpu9250Online || _bmp280Online);
    if ((_mpu6050Online || _mpu9250Online)) {
        LOG_PREFLIGHT("[SensorManager] Calibrando IMU automaticamente...\n");
    }
    LOG_PREFLIGHT("[SensorManager] Sensores online: %d\n", sensorsFound);
    return success;
}

void SensorManager::update() {
    uint32_t currentTime = millis();
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
            if (_validateMPUReadings(_gyroX, _gyroY, _gyroZ, rawAccelX, rawAccelY, rawAccelZ)) {
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
            if (!_bmp280Online) _temperature = temp;
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
    if (_consecutiveFailures >= 10) {
        LOG_ERROR("[SensorManager] Muitas falhas - reset all.\n");
        resetAll();
        _consecutiveFailures = 5;
    }
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
float SensorManager::getAccelMagnitude() { return sqrt(_accelX * _accelX + _accelY * _accelY + _accelZ * _accelZ); }
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

void SensorManager::resetAll() {
    LOG_ERROR("[SensorManager] Reiniciando todos os sensores...\n");
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

// Métodos privados e validação permanecem: _initMPU6050, _initBMP280, _validate, _applyFilter, _calculateAltitude, etc., mas sem logs e funções mortas; debug via LOG_PREFLIGHT.
