/**
 * @file ESP32_I2C.h
 * @brief ESP32 Implementation of I2C HAL
 * @details Concrete implementation using Arduino Wire library
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef ESP32_I2C_H
#define ESP32_I2C_H

#include "hal/interface/I2C.h"
#include <Wire.h>

namespace HAL {

/**
 * @class ESP32_I2C
 * @brief ESP32-specific I2C implementation
 * 
 * Wraps Arduino Wire library for platform abstraction
 */
class ESP32_I2C : public I2C {
private:
    TwoWire* wire;
    bool initialized;
    
public:
    /**
     * @brief Constructor
     * @param wireInstance Pointer to Wire or Wire1 instance
     */
    explicit ESP32_I2C(TwoWire* wireInstance = &Wire) 
        : wire(wireInstance), initialized(false) {}
    
    ~ESP32_I2C() override {
        if (initialized) {
            end();
        }
    }
    
    bool begin(uint8_t sda, uint8_t scl, uint32_t frequency) override {
        wire->begin(sda, scl, frequency);
        initialized = true;
        return true;
    }
    
    bool write(uint8_t address, const uint8_t* data, size_t length) override {
        if (!initialized) return false;
        
        wire->beginTransmission(address);
        wire->write(data, length);
        return (wire->endTransmission() == 0);
    }
    
    bool read(uint8_t address, uint8_t* data, size_t length) override {
        if (!initialized) return false;
        
        size_t received = wire->requestFrom(address, length);
        if (received != length) return false;
        
        for (size_t i = 0; i < length; i++) {
            if (wire->available()) {
                data[i] = wire->read();
            } else {
                return false;
            }
        }
        return true;
    }
    
    uint8_t readRegister(uint8_t address, uint8_t reg) override {
        if (!initialized) return 0;
        
        wire->beginTransmission(address);
        wire->write(reg);
        if (wire->endTransmission(false) != 0) return 0;
        
        if (wire->requestFrom(address, (uint8_t)1) != 1) return 0;
        return wire->read();
    }
    
    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) override {
        if (!initialized) return false;
        
        wire->beginTransmission(address);
        wire->write(reg);
        wire->write(value);
        return (wire->endTransmission() == 0);
    }
    
    void end() override {
        if (initialized) {
            wire->end();
            initialized = false;
        }
    }
};

} // namespace HAL

#endif // ESP32_I2C_H