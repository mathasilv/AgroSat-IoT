/**
 * @file MPU9250Manager.h
 * @brief MPU9250Manager - 9-axis IMU com calibração automática
 * @version 1.0.0
 * @date 2025-11-30
 * 
 * Features:
 * - Auto-calibração magnetômetro (10s com feedback)
 * - Filtro média móvel para acelerômetro
 * - Health monitoring com reinicialização automática
 * - Offsets persistentes (gyro + accel + mag)
 */

#ifndef MPU9250MANAGER_H
#define MPU9250MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "MPU9250.h"
#include "config.h"

class MPU9250Manager {
public:
    MPU9250Manager(uint8_t addr = 0x68);
    
    bool begin();
    void update();
    void reset();
    
    // GETTERS - Acelerômetro (g)
    float getAccelX() const { return _accelX; }
    float getAccelY() const { return _accelY; }
    float getAccelZ() const { return _accelZ; }
    float getAccelMagnitude() const;
    
    // GETTERS - Giroscópio (°/s)
    float getGyroX() const { return _gyroX; }
    float getGyroY() const { return _gyroY; }
    float getGyroZ() const { return _gyroZ; }
    
    // GETTERS - Magnetômetro (µT calibrado)
    float getMagX() const { return _magX; }
    float getMagY() const { return _magY; }
    float getMagZ() const { return _magZ; }
    
    // STATUS
    bool isOnline() const { return _online; }
    bool isMagOnline() const { return _magOnline; }
    bool isCalibrated() const { return _calibrated; }
    uint8_t getFailCount() const { return _failCount; }
    
    // CALIBRAÇÃO
    bool calibrateMagnetometer();
    void getMagOffsets(float& x, float& y, float& z) const;
    void setMagOffsets(float x, float y, float z);
    
    // RAW DATA (sem filtro)
    void getRawData(float& gx, float& gy, float& gz, 
                    float& ax, float& ay, float& az,
                    float& mx, float& my, float& mz) const;
    
    void printStatus() const;
    
private:
    MPU9250 _mpu;
    uint8_t _addr;
    bool _online, _magOnline, _calibrated;
    uint8_t _failCount;
    uint32_t _lastReadTime;
    
    // Dados filtrados
    float _accelX, _accelY, _accelZ;
    float _gyroX, _gyroY, _gyroZ;
    float _magX, _magY, _magZ;
    
    // Offsets de calibração
    float _magOffsetX, _magOffsetY, _magOffsetZ;
    
    // Filtro média móvel (acelerômetro)
    static constexpr uint8_t FILTER_SIZE = 5;
    float _accelXBuffer[FILTER_SIZE];
    float _accelYBuffer[FILTER_SIZE];
    float _accelZBuffer[FILTER_SIZE];
    uint8_t _filterIndex;
    float _sumAccelX, _sumAccelY, _sumAccelZ;
    
    // Constantes
    static constexpr uint32_t READ_INTERVAL = 20;  // 50Hz
    
    // Métodos privados
    bool _initMPU();
    bool _initMagnetometer();
    void _updateIMU();
    bool _validateReadings(float gx, float gy, float gz,
                           float ax, float ay, float az,
                           float mx, float my, float mz);
    float _applyFilter(float newValue, float* buffer, float& sum);
};

#endif
