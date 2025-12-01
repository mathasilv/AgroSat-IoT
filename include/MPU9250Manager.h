/**
 * @file MPU9250Manager.h
 * @brief MPU9250Manager - 9-axis IMU com calibração automática
 * @version 1.1.0
 * @date 2025-12-01
 * 
 * Features:
 * - Auto-calibração magnetômetro (10s com feedback)
 * - Persistência de offsets em EEPROM/Preferences
 * - Filtro média móvel para acelerômetro
 * - Health monitoring com reinicialização automática
 * - I2C bypass para acesso ao magnetômetro AK8963
 */

#ifndef MPU9250MANAGER_H
#define MPU9250MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>  // ← NOVO: Para ESP32 (melhor que EEPROM)
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
    
    // ← NOVO: Gerenciamento de offsets na EEPROM/Preferences
    bool saveOffsetsToMemory();           // Salvar offsets atuais
    bool loadOffsetsFromMemory();         // Carregar offsets salvos
    bool hasValidOffsetsInMemory();       // Verificar se há offsets salvos
    void clearOffsetsFromMemory();        // Limpar offsets salvos
    void printSavedOffsets() const;       // Debug: mostrar offsets salvos
    
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
    
    // ← NOVO: Preferences para persistência (ESP32)
    Preferences _prefs;
    static constexpr const char* PREFS_NAMESPACE = "mpu9250";
    static constexpr const char* KEY_OFFSET_X = "mag_offset_x";
    static constexpr const char* KEY_OFFSET_Y = "mag_offset_y";
    static constexpr const char* KEY_OFFSET_Z = "mag_offset_z";
    static constexpr const char* KEY_VALID = "mag_valid";
    static constexpr uint32_t OFFSET_MAGIC = 0xCAFEBABE;  // Magic number para validação
    
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
    bool _enableI2CBypass();
    void _updateIMU();
    bool _validateReadings(float gx, float gy, float gz,
                           float ax, float ay, float az,
                           float mx, float my, float mz);
    float _applyFilter(float newValue, float* buffer, float& sum);
    
    // ← NOVO: Validação de offsets
    bool _validateOffsets(float x, float y, float z) const;
};

#endif // MPU9250MANAGER_H
