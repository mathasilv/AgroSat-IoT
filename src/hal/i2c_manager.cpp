#include "hal/i2c_manager.h"
#include "config.h"

I2CManager::I2CManager() 
    : _wire(&Wire), _frequency(I2C_FREQUENCY), _timeout(1000), _initialized(false) {
}

I2CManager::~I2CManager() {
    end();
}

I2CManager& I2CManager::getInstance() {
    static I2CManager instance;
    return instance;
}

bool I2CManager::begin() {
    if (_initialized) return true;
    
    _wire->begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    _wire->setClock(_frequency);
    _wire->setTimeOut(_timeout);
    
    _initialized = true;
    return true;
}

void I2CManager::end() {
    if (_initialized) {
        _wire->end();
        _initialized = false;
    }
}

bool I2CManager::scan(uint8_t* devices, uint8_t maxDevices) {
    uint8_t count = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        _wire->beginTransmission(address);
        if (_wire->endTransmission() == 0) {
            if (count < maxDevices) {
                devices[count++] = address;
            }
        }
    }
    
    return count > 0;
}

uint8_t I2CManager::deviceCount() {
    uint8_t devices[10];
    uint8_t count = 0;
    scan(devices, 10);
    return count;
}

bool I2CManager::write(uint8_t address, uint8_t* data, size_t length) {
    return _wire->beginTransmission(address) == 0 &&
           _wire->write(data, length) == length &&
           _wire->endTransmission() == 0;
}

bool I2CManager::read(uint8_t address, uint8_t* data, size_t length) {
    return _wire->requestFrom(address, length) == length &&
           _wire->available() >= length;
    
    for (size_t i = 0; i < length; i++) {
        data[i] = _wire->read();
    }
    return true;
}

bool I2CManager::writeRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t length) {
    uint8_t buffer[1 + length];
    buffer[0] = reg;
    memcpy(&buffer[1], data, length);
    return write(address, buffer, 1 + length);
}

bool I2CManager::readRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t length) {
    return writeByte(address, reg) && read(address, data, length);
}

bool I2CManager::writeByte(uint8_t address, uint8_t data) {
    return write(address, &data, 1);
}

uint8_t I2CManager::readByte(uint8_t address) {
    uint8_t data;
    if (read(address, &data, 1)) {
        return data;
    }
    return 0xFF;
}

bool I2CManager::writeRegisterByte(uint8_t address, uint8_t reg, uint8_t data) {
    return writeRegister(address, reg, &data, 1);
}

uint8_t I2CManager::readRegisterByte(uint8_t address, uint8_t reg) {
    uint8_t data;
    if (readRegister(address, reg, &data, 1)) {
        return data;
    }
    return 0xFF;
}

void I2CManager::setFrequency(uint32_t freq) {
    _frequency = freq;
    if (_initialized) {
        _wire->setClock(_frequency);
    }
}

void I2CManager::setTimeout(uint16_t timeout) {
    _timeout = timeout;
    if (_initialized) {
        _wire->setTimeOut(_timeout);
    }
}

bool I2CManager::isReady() {
    return _initialized;
}