/**
 * @file UART.h
 * @brief Hardware Abstraction Layer - UART Interface
 * @details Abstract interface for UART/Serial communication
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stddef.h>

namespace HAL {

/**
 * @class UART
 * @brief Abstract base class for UART hardware abstraction
 */
class UART {
public:
    virtual ~UART() = default;
    
    /**
     * @brief Initialize UART port
     * @param baudRate Baud rate (e.g., 9600, 115200)
     * @param txPin TX pin (optional, platform-specific)
     * @param rxPin RX pin (optional, platform-specific)
     * @return true if initialization successful
     */
    virtual bool begin(uint32_t baudRate, int8_t txPin = -1, int8_t rxPin = -1) = 0;
    
    /**
     * @brief Write single byte
     * @param data Byte to write
     * @return Number of bytes written
     */
    virtual size_t write(uint8_t data) = 0;
    
    /**
     * @brief Write buffer
     * @param buffer Data buffer
     * @param length Number of bytes to write
     * @return Number of bytes written
     */
    virtual size_t write(const uint8_t* buffer, size_t length) = 0;
    
    /**
     * @brief Check if data available
     * @return Number of bytes available
     */
    virtual int available() = 0;
    
    /**
     * @brief Read single byte
     * @return Byte read (-1 if no data)
     */
    virtual int read() = 0;
    
    /**
     * @brief Read buffer
     * @param buffer Receive buffer
     * @param length Maximum bytes to read
     * @return Number of bytes read
     */
    virtual size_t read(uint8_t* buffer, size_t length) = 0;
    
    /**
     * @brief Flush transmit buffer
     */
    virtual void flush() = 0;
    
    /**
     * @brief Deinitialize UART port
     */
    virtual void end() = 0;
};

} // namespace HAL

#endif // HAL_UART_H