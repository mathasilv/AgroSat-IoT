#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <HAL/interface/I2C.h>
#include "MPU9250Hal.h"
#include "BMP280Hal.h"
#include "SI7021Hal.h"
#include "CCS811Hal.h"
#include "IMUService.h"
#include "EnvService.h"
#include "AirQualityService.h"
#include "config.h"

class SensorManager {
public:
    explicit SensorManager(HAL::I2C& i2c);

    bool begin();
    void update();
    void resetAll();
    void scanI2C();

    // Getters de temperatura
    float getTemperature();
    float getTemperatureSI7021();
    float getTemperatureBMP280();

    // Getters de sensores
    float getPressure();
    float getAltitude();
    float getGyroX();
    float getGyroY();
    float getGyroZ();
    float getAccelX();
    float getAccelY();
    float getAccelZ();
    float getAccelMagnitude();
    float getMagX();
    float getMagY();
    float getMagZ();
    float getHumidity();
    float getCO2();
    float getTVOC();

    // Status dos sensores
    bool isMPU9250Online();
    bool isBMP280Online();
    bool isSI7021Online();
    bool isCCS811Online();
    bool isCalibrated();

    // Utilitários
    void  getRawData(float& gx, float& gy, float& gz,
                     float& ax, float& ay, float& az);
    void  printSensorStatus();
    bool  calibrateIMU();
    void  forceReinitBMP280();   // ainda opcional, pode delegar ao EnvService

private:
    HAL::I2C* _i2c;

    // Drivers HAL + serviços
    MPU9250Hal       _mpu9250;
    IMUService       _imuService;
    BMP280Hal        _bmp280;
    SI7021Hal        _si7021;
    CCS811Hal        _ccs811;
    EnvService       _envService;
    AirQualityService _airService;

    // DADOS DOS SENSORES
    float _temperature;       // redundante (vindo do EnvService)
    float _temperatureBMP;
    float _temperatureSI;
    float _pressure;
    float _altitude;
    float _humidity;
    float _co2Level;
    float _tvoc;
    float _seaLevelPressure;

    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ;
    float _magX, _magY, _magZ;

    // STATUS DOS SENSORES
    bool _mpu9250Online;
    bool _bmp280Online;
    bool _si7021Online;
    bool _ccs811Online;
    bool _calibrated;

    // Controle de validade/env
    bool    _si7021TempValid;
    bool    _bmp280TempValid;
    uint8_t _si7021TempFailures;
    uint8_t _bmp280TempFailures;
    static constexpr uint8_t MAX_TEMP_FAILURES = 5;

    // CONTROLE DE TEMPO GLOBAL / HEALTH
    unsigned long _lastReadTime;
    unsigned long _lastHealthCheck;
    uint8_t       _consecutiveFailures;

    // Health / BMP280 (mantidos só para delegar/monitorar EnvService se quiser)
    unsigned long _lastBMP280Reinit;
    uint8_t       _bmp280FailCount;

    // Chamadas internas
    void _updateIMU();
    void _updateEnv();       // chama EnvService
    void _updateCCS811();

    bool  _initCCS811();
    bool  _validateReading(float value, float minValid, float maxValid);
    void  _performHealthCheck();
    float _calculateAltitude(float pressure);  // mantido por compatibilidade
    void  _updateTemperatureRedundancy();      // hoje delegado ao EnvService
};

#endif // SENSORMANAGER_H
