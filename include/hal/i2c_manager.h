#pragma once

#include <Arduino.h>
#include <Wire.h>

/**
 * @file i2c_manager.h
 * @brief HAL para comunicação I2C no AgroSat-IoT
 * @version 1.0.0
 */

class I2CManager {
public:
    static I2CManager& getInstance();
    
    bool begin();
    void end();
    
    bool scan(uint8_t* devices, uint8_t maxDevices);
    uint8_t deviceCount();
    
    // Transações básicas
    bool write(uint8_t address, uint8_t* data, size_t length);
    bool read(uint8_t address, uint8_t* data, size_t length);
    bool writeRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t length);
    bool readRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t length);
    
    // Transações comuns
    bool writeByte(uint8_t address, uint8_t data);
    uint8_t readByte(uint8_t address);
    bool writeRegisterByte(uint8_t address, uint8_t reg, uint8_t data);
    uint8_t readRegisterByte(uint8_t address, uint8_t reg);
    
    // Controle
    void setFrequency(uint32_t freq);
    void setTimeout(uint16_t timeout);
    
    bool isReady();
    
private:
    I2CManager();
    ~I2CManager();
    
    I2CManager(const I2CManager&) = delete;
    I2CManager& operator=(const I2CManager&) = delete;
    
    TwoWire* _wire;
    uint32_t _frequency;
    uint16_t _timeout;
    bool _initialized;
};