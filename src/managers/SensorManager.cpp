/**
 * @file SensorManager.cpp
 * @brief Gerenciamento de sensores PION - VERSÃO OTIMIZADA
 * @version 4.0.0
 * @date 2025-11-11
 */

#include "SensorManager.h"
#include <algorithm>
#include "DisplayManager.h"
#include <string.h>

SensorManager::SensorManager(HAL::I2C& i2c) :
    _i2c(&i2c),
    _mpu9250(i2c, MPU9250_ADDRESS),
    _imuService(_mpu9250),
    _bmp280(i2c),
    _si7021(i2c),
    _ccs811(i2c),
    _envService(_bmp280, _si7021, i2c),
    _airService(_ccs811),
    _temperature(NAN), _temperatureBMP(NAN), _temperatureSI(NAN),
    _pressure(NAN), _altitude(NAN),
    _humidity(NAN), _co2Level(NAN), _tvoc(NAN),
    _seaLevelPressure(1013.25f),
    _gyroX(0.0f), _gyroY(0.0f), _gyroZ(0.0f),
    _accelX(0.0f), _accelY(0.0f), _accelZ(0.0f),
    _magX(0.0f), _magY(0.0f), _magZ(0.0f),
    _mpu9250Online(false), _bmp280Online(false),
    _si7021Online(false), _ccs811Online(false),
    _calibrated(false),
    _si7021TempValid(false), _bmp280TempValid(false),
    _si7021TempFailures(0), _bmp280TempFailures(0),
    _lastReadTime(0),
    _lastHealthCheck(0),
    _lastBMP280Reinit(0),
    _bmp280FailCount(0)
{
}

bool SensorManager::begin() {
    DEBUG_PRINTLN("[SensorManager] Inicializando sensores PION...");
    bool success = false;
    uint8_t sensorsFound = 0;

    // MPU9250 / IMU
    _mpu9250Online = _imuService.begin();
    if (_mpu9250Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] MPU9250 ONLINE (9-axis)");
        _calibrated = _imuService.isCalibrated();
    }

    // Ambiente (BMP280 + SI7021)
    bool envOk = _envService.begin();
    _bmp280Online = _envService.isBmpOnline();
    _si7021Online = _envService.isSiOnline();
    _bmp280TempValid = _envService.isBmpTempValid();
    _si7021TempValid = _envService.isSiTempValid();

    if (_bmp280Online) {
        sensorsFound++;
        success = true;
        DEBUG_PRINTLN("[SensorManager] BMP280: ONLINE");
    }
    if (_si7021Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] SI7021: ONLINE");
    }

    // CCS811 (qualidade do ar)
    _ccs811Online = _initCCS811();
    if (_ccs811Online) {
        sensorsFound++;
        DEBUG_PRINTLN("[SensorManager] CCS811: ONLINE");
    }

    // inicializa cache com dados do EnvService
    _updateEnv();

    DEBUG_PRINTF("[SensorManager] %d/4 sensores detectados\n", sensorsFound);
    return success || envOk;
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
        _updateEnv();
        _updateCCS811();
    }
}

// ============ IMU ============

void SensorManager::_updateIMU() {
    if (!_mpu9250Online) return;

    _imuService.update();

    _gyroX  = _imuService.getGyroX();
    _gyroY  = _imuService.getGyroY();
    _gyroZ  = _imuService.getGyroZ();

    _accelX = _imuService.getAccelX();
    _accelY = _imuService.getAccelY();
    _accelZ = _imuService.getAccelZ();

    _magX   = _imuService.getMagX();
    _magY   = _imuService.getMagY();
    _magZ   = _imuService.getMagZ();
}

// ============ Ambiente (EnvService) ============

void SensorManager::_updateEnv() {
    _envService.update();

    // espelha dados do serviço
    _temperature    = _envService.getTemperature();
    _temperatureBMP = _envService.getBmpTemperature();
    _temperatureSI  = _envService.getSiTemperature();
    _pressure       = _envService.getPressure();
    _altitude       = _envService.getAltitude();
    _humidity       = _envService.getHumidity();

    _bmp280Online     = _envService.isBmpOnline();
    _si7021Online     = _envService.isSiOnline();
    _bmp280TempValid  = _envService.isBmpTempValid();
    _si7021TempValid  = _envService.isSiTempValid();
}

// ============ CCS811 / AirQualityService ============

void SensorManager::_updateCCS811() {
    if (!_ccs811Online) return;

    float tempComp = !isnan(_temperatureSI) ? _temperatureSI : _temperatureBMP;
    float humComp  = _humidity;

    _airService.update(tempComp, humComp);

    _co2Level = _airService.getCO2();
    _tvoc     = _airService.getTVOC();
}

bool SensorManager::_initCCS811() {
    DEBUG_PRINTLN("[SensorManager] Inicializando CCS811 via AirQualityService...");
    float tempComp = !isnan(_temperatureSI) ? _temperatureSI : _temperatureBMP;
    float humComp  = _humidity;
    return _airService.begin(tempComp, humComp);
}

// ============ Getters públicos ============

float SensorManager::getTemperature()          { return _temperature; }
float SensorManager::getTemperatureSI7021()    { return _temperatureSI; }
float SensorManager::getTemperatureBMP280()    { return _temperatureBMP; }
float SensorManager::getPressure()             { return _pressure; }
float SensorManager::getAltitude()             { return _altitude; }
float SensorManager::getGyroX()                { return _gyroX; }
float SensorManager::getGyroY()                { return _gyroY; }
float SensorManager::getGyroZ()                { return _gyroZ; }
float SensorManager::getAccelX()               { return _accelX; }
float SensorManager::getAccelY()               { return _accelY; }
float SensorManager::getAccelZ()               { return _accelZ; }
float SensorManager::getAccelMagnitude() {
    return sqrtf(_accelX*_accelX + _accelY*_accelY + _accelZ*_accelZ);
}
float SensorManager::getMagX()                 { return _magX; }
float SensorManager::getMagY()                 { return _magY; }
float SensorManager::getMagZ()                 { return _magZ; }
float SensorManager::getHumidity()             { return _humidity; }
float SensorManager::getCO2()                  { return _co2Level; }
float SensorManager::getTVOC()                 { return _tvoc; }

bool SensorManager::isMPU9250Online()          { return _mpu9250Online; }
bool SensorManager::isBMP280Online()           { return _bmp280Online; }
bool SensorManager::isSI7021Online()           { return _si7021Online; }
bool SensorManager::isCCS811Online()           { return _ccs811Online; }
bool SensorManager::isCalibrated()             { return _calibrated; }

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

    DEBUG_PRINTLN("\n  Redundância de Temperatura:");
    if (_si7021TempValid) {
        DEBUG_PRINTF("    Usando SI7021 (%.2f°C)\n", _temperatureSI);
    } else if (_bmp280TempValid) {
        DEBUG_PRINTF("    Usando BMP280 (%.2f°C) - SI7021 falhou\n", _temperatureBMP);
    } else {
        DEBUG_PRINTLN("    CRÍTICO: Ambos sensores falharam!");
    }
}

// ============ Controle / Health ============

void SensorManager::resetAll() {
    _mpu9250Online = _imuService.begin();
    _envService.begin();  // reinit BMP280 + SI7021
    _bmp280Online   = _envService.isBmpOnline();
    _si7021Online   = _envService.isSiOnline();
    _ccs811Online   = _initCCS811();
    _consecutiveFailures = 0;
}

bool SensorManager::_validateReading(float value, float minValid, float maxValid) {
    if (isnan(value)) return false;
    if (value < minValid || value > maxValid) return false;
    if (value == 0.0f || value == -273.15f)  return false;
    return true;
}

void SensorManager::_performHealthCheck() {
    // Exemplo simples: delegar lógica pesada para EnvService no futuro
    // Aqui você pode continuar usando contadores globais se quiser
}

float SensorManager::_calculateAltitude(float pressure) {
    if (pressure <= 0.0f) return 0.0f;
    float ratio = pressure / _seaLevelPressure;
    return 44330.0f * (1.0f - powf(ratio, 0.1903f));
}

bool SensorManager::calibrateIMU() {
    if (!_mpu9250Online) return false;
    return _imuService.begin(); // inclui calibração de mag
}

void SensorManager::forceReinitBMP280() {
    DEBUG_PRINTLN("[SensorManager] Reinicialização forçada do BMP280 via EnvService...");
    _envService.begin();   // ou criar um método específico em EnvService
}

void SensorManager::scanI2C() {
    DEBUG_PRINTLN("[SensorManager] Scanning I2C bus via HAL...");
    if (!_i2c) return;

    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t val = _i2c->readRegister(addr, 0x00);
        DEBUG_PRINTF("  Device at 0x%02X (val=0x%02X)\n", addr, val);
        count++;
    }
    DEBUG_PRINTF("[SensorManager] Found %d devices\n", count);
}

void SensorManager::_updateTemperatureRedundancy() {
    // Agora a redundância é feita dentro do EnvService,
    // este método pode ser mantido vazio ou removido se não for mais usado.
}
