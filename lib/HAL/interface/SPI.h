/**
 * @file SPI.h
 * @brief Hardware Abstraction Layer - SPI Interface
 * @details Abstract interface for SPI communication (LoRa radio)
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>
#include <stddef.h>

namespace HAL {

/**
 * @class SPI
 * @brief Abstract base class for SPI hardware abstraction
 * 
 * Used primarily for LoRa radio (SX127x) communication
 */
class SPI {
public:
    virtual ~SPI() = default;
    
    /**
     * @brief Initialize SPI bus
     * @param sck SCK pin
     * @param miso MISO pin
     * @param mosi MOSI pin
     * @param frequency Bus frequency in Hz
     * @return true if initialization successful
     */
    virtual bool begin(uint8_t sck, uint8_t miso, uint8_t mosi, uint32_t frequency) = 0;
    
    /**
     * @brief Transfer single byte (write and read simultaneously)
     * @param data Byte to send
     * @return Received byte
     */
    virtual uint8_t transfer(uint8_t data) = 0;
    
    /**
     * @brief Transfer multiple bytes
     * @param txBuffer Transmit buffer
     * @param rxBuffer Receive buffer (can be nullptr)
     * @param length Number of bytes to transfer
     */
    virtual void transfer(const uint8_t* txBuffer, uint8_t* rxBuffer, size_t length) = 0;
    
    /**
     * @brief Begin SPI transaction (set CS low)
     * @param csPin Chip select pin
     */
    virtual void beginTransaction(uint8_t csPin) = 0;
    
    /**
     * @brief End SPI transaction (set CS high)
     * @param csPin Chip select pin
     */
    virtual void endTransaction(uint8_t csPin) = 0;
    
    /**
     * @brief Deinitialize SPI bus
     */
    virtual void end() = 0;
};

} // namespace HAL

#endif // HAL_SPI_H