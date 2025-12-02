/**
 * @file CCS811.cpp
 * @brief Implementação Driver CCS811
 */

#include "CCS811.h"

CCS811::CCS811(TwoWire& wire) : _wire(&wire), _addr(0), _eco2(0), _tvoc(0) {}

bool CCS811::begin(uint8_t addr) {
    _addr = addr;

    // 1. Reset SW (Sequência mágica do datasheet)
    reset();
    delay(100);

    // 2. Verificar ID
    uint8_t id = 0;
    if (!_readRegister(REG_HW_ID, &id) || id != HW_ID_CODE) {
        return false;
    }

    // 3. Iniciar App
    _wire->beginTransmission(_addr);
    _wire->write(REG_APP_START);
    if (_wire->endTransmission() != 0) {
        return false;
    }
    
    delay(100);
    
    // 4. Configurar Mode 1 (1s)
    return setDriveMode(DriveMode::MODE_1SEC);
}

void CCS811::reset() {
    _wire->beginTransmission(_addr);
    _wire->write(REG_SW_RESET);
    _wire->write(0x11);
    _wire->write(0xE5);
    _wire->write(0x72);
    _wire->write(0x8A);
    _wire->endTransmission();
}

bool CCS811::setDriveMode(DriveMode mode) {
    return _writeRegister(REG_MEAS_MODE, (uint8_t)mode);
}

bool CCS811::setEnvironmentalData(float humidity, float temperature) {
    // Humidade: 7 bits inteiros + 9 fracionários (0-100%)
    // Temp: offset +25, 7 bits inteiros + 9 fracionários (-25 a +100C)
    
    uint16_t hum = (uint16_t)(humidity * 512);
    uint16_t temp = (uint16_t)((temperature + 25) * 512);

    uint8_t data[4];
    data[0] = (hum >> 8) & 0xFF;
    data[1] = hum & 0xFF;
    data[2] = (temp >> 8) & 0xFF;
    data[3] = temp & 0xFF;

    return _writeRegisters(REG_ENV_DATA, data, 4);
}

bool CCS811::available() {
    uint8_t status = 0;
    if (!_readRegister(REG_STATUS, &status)) return false;
    return (status & 0x08); // Bit 3: DATA_READY
}

bool CCS811::readData() {
    if (!available()) return false;

    uint8_t buf[4];
    // Ler 4 bytes (eCO2 MSB, eCO2 LSB, TVOC MSB, TVOC LSB)
    if (!_readRegisters(REG_ALG_RESULT_DATA, buf, 4)) return false;

    _eco2 = ((uint16_t)buf[0] << 8) | buf[1];
    _tvoc = ((uint16_t)buf[2] << 8) | buf[3];

    return true;
}

uint8_t CCS811::getError() {
    uint8_t err = 0;
    _readRegister(REG_ERROR_ID, &err);
    return err;
}

bool CCS811::getBaseline(uint16_t& baseline) {
    uint8_t buf[2];
    if (!_readRegisters(REG_BASELINE, buf, 2)) return false;
    baseline = ((uint16_t)buf[0] << 8) | buf[1];
    return true;
}

bool CCS811::setBaseline(uint16_t baseline) {
    uint8_t buf[2];
    buf[0] = (baseline >> 8) & 0xFF;
    buf[1] = baseline & 0xFF;
    return _writeRegisters(REG_BASELINE, buf, 2);
}

// === I2C Helpers ===

bool CCS811::_writeRegister(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write(value);
    return (_wire->endTransmission() == 0);
}

bool CCS811::_writeRegisters(uint8_t reg, uint8_t* data, uint8_t len) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    for(uint8_t i=0; i<len; i++) _wire->write(data[i]);
    return (_wire->endTransmission() == 0);
}

bool CCS811::_readRegister(uint8_t reg, uint8_t* val) {
    return _readRegisters(reg, val, 1);
}

bool CCS811::_readRegisters(uint8_t reg, uint8_t* buf, uint8_t len) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission() != 0) return false;

    if (_wire->requestFrom(_addr, len) != len) return false;
    for(uint8_t i=0; i<len; i++) buf[i] = _wire->read();
    return true;
}