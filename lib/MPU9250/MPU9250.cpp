/**
 * @file MPU9250.cpp
 * @brief Implementação Driver MPU9250
 */

#include "MPU9250.h"

MPU9250::MPU9250(uint8_t addr) 
    : _addr(addr), _accScale(1.0f), _gyroScale(1.0f), _magScale(0.15f), _magInitialized(false) 
{}

bool MPU9250::begin() {
    // 1. Verificar ID
    uint8_t who = _readRegister(_addr, REG_WHO_AM_I);
    if (who != 0x71 && who != 0x73) return false; // 0x71=MPU9250, 0x73=MPU9255

    // 2. Reset e Wakeup
    _writeRegister(_addr, REG_PWR_MGMT_1, 0x80); // Reset
    delay(100);
    _writeRegister(_addr, REG_PWR_MGMT_1, 0x01); // Clock Source Auto
    delay(100);

    // 3. Configurações Padrão
    _writeRegister(_addr, REG_SMPLRT_DIV, 0x00); 
    _writeRegister(_addr, REG_CONFIG, 0x03);      // DLPF ~41Hz
    
    // Configurar Ranges (16g, 2000dps)
    setAccRange(3); 
    setGyrRange(3);
    
    // Ativar Bypass para Magnetômetro
    enableI2CBypass(true);
    
    return true;
}

bool MPU9250::initMagnetometer() {
    // Verificar ID do AK8963
    uint8_t who = _readRegister(AK8963_ADDR, AK8963_WIA);
    if (who != 0x48) return false;

    // Reset AK8963
    _writeRegister(AK8963_ADDR, AK8963_CNTL1, 0x00); // Power down
    delay(10);
    _writeRegister(AK8963_ADDR, AK8963_CNTL1, 0x16); // Modo Contínuo 2 (100Hz), 16-bit
    delay(10);

    _magInitialized = true;
    return true;
}

void MPU9250::enableI2CBypass(bool enable) {
    uint8_t val = _readRegister(_addr, REG_INT_PIN_CFG);
    if (enable) val |= 0x02;
    else val &= ~0x02;
    _writeRegister(_addr, REG_INT_PIN_CFG, val);
}

void MPU9250::setAccRange(uint8_t range) {
    _writeRegister(_addr, REG_ACCEL_CONFIG, range << 3);
    switch(range) {
        case 0: _accScale = 2.0f / 32768.0f; break;
        case 1: _accScale = 4.0f / 32768.0f; break;
        case 2: _accScale = 8.0f / 32768.0f; break;
        case 3: _accScale = 16.0f / 32768.0f; break;
    }
}

void MPU9250::setGyrRange(uint8_t range) {
    _writeRegister(_addr, REG_GYRO_CONFIG, range << 3);
    switch(range) {
        case 0: _gyroScale = 250.0f / 32768.0f; break;
        case 1: _gyroScale = 500.0f / 32768.0f; break;
        case 2: _gyroScale = 1000.0f / 32768.0f; break;
        case 3: _gyroScale = 2000.0f / 32768.0f; break;
    }
}

xyzFloat MPU9250::getGValues() {
    uint8_t buf[6];
    if (!_readBytes(_addr, REG_ACCEL_XOUT_H, buf, 6)) return {0,0,0};
    
    int16_t x = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t y = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t z = (int16_t)((buf[4] << 8) | buf[5]);
    
    return {x * _accScale, y * _accScale, z * _accScale};
}

xyzFloat MPU9250::getGyrValues() {
    uint8_t buf[6];
    if (!_readBytes(_addr, REG_GYRO_XOUT_H, buf, 6)) return {0,0,0};
    
    int16_t x = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t y = (int16_t)((buf[2] << 8) | buf[3]);
    int16_t z = (int16_t)((buf[4] << 8) | buf[5]);
    
    return {x * _gyroScale, y * _gyroScale, z * _gyroScale};
}

xyzFloat MPU9250::getMagValues() {
    if (!_magInitialized) return {0,0,0};

    // Verificar DRDY
    uint8_t st1 = _readRegister(AK8963_ADDR, AK8963_ST1);
    if (!(st1 & 0x01)) return {0,0,0}; // Sem dados novos

    uint8_t buf[7];
    if (!_readBytes(AK8963_ADDR, AK8963_HXL, buf, 7)) return {0,0,0};
    
    // Verificar Overflow Magnético (HOFL bit)
    if (buf[6] & 0x08) return {0,0,0};

    // AK8963 é Little Endian
    int16_t x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t z = (int16_t)((buf[5] << 8) | buf[4]);

    return {x * _magScale, y * _magScale, z * _magScale};
}

// === Helpers I2C ===

bool MPU9250::_writeRegister(uint8_t addr, uint8_t reg, uint8_t data) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(data);
    return (Wire.endTransmission() == 0);
}

uint8_t MPU9250::_readRegister(uint8_t addr, uint8_t reg) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return 0xFF;
    
    Wire.requestFrom(addr, (uint8_t)1);
    if (Wire.available()) return Wire.read();
    return 0xFF;
}

bool MPU9250::_readBytes(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    
    if (Wire.requestFrom(addr, len) != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}