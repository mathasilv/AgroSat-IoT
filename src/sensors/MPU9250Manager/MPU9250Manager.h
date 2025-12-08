/**
 * @file MPU9250Manager.h
 * @brief Gerenciador MPU9250 com Calibração Hard + Soft Iron
 * @version 2.0.0 (MODERADO 4.7 - Soft Iron Distortion Corrigida)
 */

#ifndef MPU9250MANAGER_H
#define MPU9250MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include "MPU9250.h"
#include "config.h"

class MPU9250Manager {
public:
    MPU9250Manager(uint8_t addr = 0x69);

    bool begin();
    void update();
    void reset();

    // Getters Calibrados
    float getAccelX() const { return _accelX; }
    float getAccelY() const { return _accelY; }
    float getAccelZ() const { return _accelZ; }
    
    float getGyroX() const { return _gyroX; }
    float getGyroY() const { return _gyroY; }
    float getGyroZ() const { return _gyroZ; }
    
    float getMagX() const { return _magX; }
    float getMagY() const { return _magY; }
    float getMagZ() const { return _magZ; }

    // Raw Data
    void getRawData(float& gx, float& gy, float& gz, 
                    float& ax, float& ay, float& az,
                    float& mx, float& my, float& mz) const;

    // Status
    bool isOnline() const { return _online; }
    bool isMagOnline() const { return _magOnline; }
    bool isCalibrated() const { return _calibrated; }
    uint8_t getFailCount() const { return _failCount; }

    // CORRIGIDO 4.7: Calibração Hard + Soft Iron
    bool calibrateMagnetometer();
    void getMagOffsets(float& x, float& y, float& z) const;
    void getSoftIronMatrix(float matrix[3][3]) const;
    void clearOffsetsFromMemory();

private:
    MPU9250 _mpu;
    uint8_t _addr;
    
    bool _online, _magOnline, _calibrated;
    uint8_t _failCount;
    unsigned long _lastRead;

    // Dados Processados
    float _accelX, _accelY, _accelZ;
    float _gyroX, _gyroY, _gyroZ;
    float _magX, _magY, _magZ;

    // Hard Iron Offsets (bias)
    float _magOffX, _magOffY, _magOffZ;
    
    // NOVO 4.7: Soft Iron Distortion Matrix (3x3)
    float _softIronMatrix[3][3];

    // Filtro Média Móvel (Acelerômetro)
    static constexpr uint8_t FILTER_SIZE = 5;
    float _bufAX[FILTER_SIZE], _bufAY[FILTER_SIZE], _bufAZ[FILTER_SIZE];
    uint8_t _filterIdx;
    
    // Preferences para salvar calibração
    Preferences _prefs;
    static constexpr const char* PREFS_NAME = "mpu_mag";
    static constexpr uint32_t MAGIC_KEY = 0xCAFEBABE;

    bool _loadOffsets();
    bool _saveOffsets();
    float _applyFilter(float val, float* buf);
    
    // NOVO 4.7: Aplicar correção Soft Iron
    void _applySoftIronCorrection(float& mx, float& my, float& mz);
    void _calculateSoftIronMatrix(float samples[][3], int numSamples);
};

#endif