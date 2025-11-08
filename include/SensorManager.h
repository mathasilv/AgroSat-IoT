/**
 * @file SensorManager.h
 * @brief API limpa para sensores (sem funções mortas, só o essencial para missão OBSAT)
 * @version 2.3.0
 */

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "mission_config.h"
// === BIBLIOTECAS DE SENSORES ===
#include <MPU6050_light.h>
#include <Adafruit_BMP280.h>
#ifdef USE_MPU9250
    #include <MPU9250_WE.h>
#endif
#ifdef USE_SHT20
    #include <SHT2x.h>
#endif
#ifdef USE_CCS811
    #include <Adafruit_CCS811.h>
#endif

class SensorManager {
public:
    SensorManager();
    bool begin();
    void update();
    // Getters principais
    float getTemperature();
    float getPressure();
    float getAltitude();
    float getGyroX();
    float getGyroY();
    float getGyroZ();
    float getAccelX();
    float getAccelY();
    float getAccelZ();
    float getAccelMagnitude();
    // Getters expandidos
    float getHumidity();
    float getCO2();
    float getTVOC();
    float getMagX();
    float getMagY();
    float getMagZ();
    // Status dos sensores
    bool isMPU6050Online();
    bool isMPU9250Online();
    bool isBMP280Online();
    bool isSHT20Online();
    bool isCCS811Online();
    // Utilidades
    void resetAll(); // Apenas reset global
    // Compatibilidade antiga
    void getRawData(float& gx, float& gy, float& gz, float& ax, float& ay, float& az);
private:
    // Instâncias dos sensores
    MPU6050 _mpu6050;
    Adafruit_BMP280 _bmp280;
#ifdef USE_MPU9250
    MPU9250_WE _mpu9250;
#endif
#ifdef USE_SHT20
    SHT2x _sht20;
#endif
#ifdef USE_CCS811
    Adafruit_CCS811 _ccs811;
#endif
    // Dados dos sensores
    float _temperature, _pressure, _altitude;
    float _humidity, _co2Level, _tvoc;
    float _seaLevelPressure;
    float _gyroX, _gyroY, _gyroZ;
    float _accelX, _accelY, _accelZ;
    float _magX, _magY, _magZ;
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;
    float _accelOffsetX, _accelOffsetY, _accelOffsetZ;
    bool _mpu6050Online, _mpu9250Online;
    bool _bmp280Online, _sht20Online, _ccs811Online;
    uint32_t _lastReadTime, _lastCCS811Read, _lastSHT20Read, _lastHealthCheck;
    uint8_t _consecutiveFailures;
    float _accelXBuffer[CUSTOM_FILTER_SIZE];
    float _accelYBuffer[CUSTOM_FILTER_SIZE];
    float _accelZBuffer[CUSTOM_FILTER_SIZE];
    uint8_t _filterIndex;
    // Métodos privados - Inicialização
    bool _initMPU6050();
    bool _initBMP280();
#ifdef USE_MPU9250
    bool _initMPU9250();
#endif
#ifdef USE_SHT20
    bool _initSHT20();
#endif
#ifdef USE_CCS811
    bool _initCCS811();
#endif
    // Métodos privados - Validação
    bool _validateMPUReadings(float gx, float gy, float gz, float ax, float ay, float az);
    bool _validateBMPReadings(float temperature, float pressure);
    bool _validateSHTReadings(float temperature, float humidity);
    bool _validateCCSReadings(float co2, float tvoc);
    float _applyFilter(float newValue, float* buffer);
    float _calculateAltitude(float pressure);
};

#endif // SENSORMANAGER_H
