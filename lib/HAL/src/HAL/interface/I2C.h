/**
 * @file I2C.h
 * @brief Hardware Abstraction Layer - I2C Interface
 * @details Abstract interface for I2C communication, platform-independent
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>
#include <stddef.h>

namespace HAL {

/**
 * @class I2C
 * @brief Abstract base class for I2C hardware abstraction
 * 
 * This interface provides platform-independent I2C operations.
 * Concrete implementations (ESP32_I2C, STM32_I2C) will override these methods.
 */
class I2C {
public:
    virtual ~I2C() = default;
    
    /**
     * @brief Initialize I2C bus
     * @param sda SDA pin number (platform-specific)
     * @param scl SCL pin number (platform-specific)
     * @param frequency Bus frequency in Hz (e.g., 100000, 400000)
     * @return true if initialization successful
     */
    virtual bool begin(uint8_t sda, uint8_t scl, uint32_t frequency) = 0;
    
    /**
     * @brief Write data to I2C device
     * @param address 7-bit I2C device address
     * @param data Pointer to data buffer
     * @param length Number of bytes to write
     * @return true if write successful
     */
    virtual bool write(uint8_t address, const uint8_t* data, size_t length) = 0;
    
    /**
     * @brief Read data from I2C device
     * @param address 7-bit I2C device address
     * @param data Pointer to receive buffer
     * @param length Number of bytes to read
     * @return true if read successful
     */
    virtual bool read(uint8_t address, uint8_t* data, size_t length) = 0;
    
    /**
     * @brief Read single register from I2C device
     * @param address 7-bit I2C device address
     * @param reg Register address
     * @return Register value (0 on error)
     */
    virtual uint8_t readRegister(uint8_t address, uint8_t reg) = 0;
    
    /**
     * @brief Write single register to I2C device
     * @param address 7-bit I2C device address
     * @param reg Register address
     * @param value Value to write
     * @return true if write successful
     */
    virtual bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) = 0;
    
    /**
     * @brief Deinitialize I2C bus and free resources
     */
    virtual void end() = 0;
};

} // namespace HAL

#endif // HAL_I2C_H