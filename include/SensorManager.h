#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <MPU9250_WE.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_CCS811.h>
#include "config.h"

class SensorManager {
public:
    SensorManager();
    
    bool begin();
    void update();
    void resetAll();
    
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
    bool isMPU6050Online();
    bool isMPU9250Online();
    bool isBMP280Online();
    bool isSI7021Online();
    bool isCCS811Online();
    bool isCalibrated();
    
    // Utilitários
    void getRawData(float& gx, float& gy, float& gz,
                    float& ax, float& ay, float& az);
    void printSensorStatus();
    bool calibrateIMU();
    void scanI2C();

private:
    // ========================================
    // OBJETOS DE HARDWARE
    // ========================================
    MPU9250_WE _mpu9250;
    Adafruit_BMP280 _bmp280;
    Adafruit_CCS811 _ccs811;
    // Nota: SI7021 usa Wire.h diretamente (sem objeto de biblioteca)
    
    // ========================================
    // DADOS DOS SENSORES
    // ========================================
    float _temperature;        // Temperatura final (com fallback automático)
    float _temperatureBMP;     // Temperatura do BMP280
    float _temperatureSI;      // Temperatura do SI7021
    float _pressure;           // Pressão atmosférica (hPa)
    float _altitude;           // Altitude calculada (m)
    float _humidity;           // Umidade relativa (%)
    float _co2Level;           // CO2 (ppm)
    float _tvoc;               // TVOC (ppb)
    float _seaLevelPressure;   // Pressão ao nível do mar (hPa)
    
    // IMU - Giroscópio (°/s)
    float _gyroX, _gyroY, _gyroZ;
    
    // IMU - Acelerômetro (g)
    float _accelX, _accelY, _accelZ;
    
    // IMU - Magnetômetro (µT)
    float _magX, _magY, _magZ;
    float _magOffsetX, _magOffsetY, _magOffsetZ;  // Offsets de calibração
    
    // ========================================
    // STATUS DOS SENSORES
    // ========================================
    bool _mpu9250Online;
    bool _bmp280Online;
    bool _si7021Online;
    bool _ccs811Online;
    bool _calibrated;
    
    // Validação de temperatura redundante
    bool _si7021TempValid;
    bool _bmp280TempValid;
    uint8_t _si7021TempFailures;
    uint8_t _bmp280TempFailures;
    static constexpr uint8_t MAX_TEMP_FAILURES = 5;
    
    // ========================================
    // CONTROLE DE TEMPO
    // ========================================
    unsigned long _lastReadTime;
    unsigned long _lastCCS811Read;
    unsigned long _lastSI7021Read;
    unsigned long _lastHealthCheck;
    uint8_t _consecutiveFailures;
    
    // ========================================
    // FILTRO DE MÉDIA MÓVEL
    // ========================================
    // Nota: CUSTOM_FILTER_SIZE definido em config.h
    float _accelXBuffer[CUSTOM_FILTER_SIZE];
    float _accelYBuffer[CUSTOM_FILTER_SIZE];
    float _accelZBuffer[CUSTOM_FILTER_SIZE];
    uint8_t _filterIndex;
    float _sumAccelX, _sumAccelY, _sumAccelZ;
    
    // ========================================
    // MÉTODOS INTERNOS - INICIALIZAÇÃO
    // ========================================
    bool _initMPU9250();
    bool _initBMP280();
    bool _initSI7021();
    bool _initCCS811();
    
    // ========================================
    // MÉTODOS INTERNOS - ATUALIZAÇÃO
    // ========================================
    void _updateIMU();
    void _updateBMP280();
    void _updateSI7021();
    void _updateCCS811();
    void _updateTemperatureRedundancy();
    
    // ========================================
    // MÉTODOS INTERNOS - VALIDAÇÃO
    // ========================================
    // ✅ VALIDAÇÃO CONSOLIDADA (NOVA)
    bool _validateReading(float value, float minValid, float maxValid);
    
    // Validações específicas (mantidas para compatibilidade)
    bool _validateMPUReadings(float gx, float gy, float gz,
                              float ax, float ay, float az,
                              float mx, float my, float mz);
    bool _validateCCSReadings(float co2, float tvoc);
    
    // ========================================
    // OUTROS MÉTODOS INTERNOS
    // ========================================
    void _performHealthCheck();
    bool _calibrateMPU9250();
    float _applyFilter(float newValue, float* buffer, float& sum);
    float _calculateAltitude(float pressure);
};

#endif // SENSORMANAGER_H
