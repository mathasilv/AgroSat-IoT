/**
 * @file ESP32_SPI.h
 * @brief ESP32 Implementation of SPI HAL
 * @details Concrete implementation for LoRa radio communication
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef ESP32_SPI_H
#define ESP32_SPI_H

#include "hal/interface/SPI.h"
#include <SPI.h>

namespace HAL {

/**
 * @class ESP32_SPI
 * @brief ESP32-specific SPI implementation
 */
class ESP32_SPI : public HAL::SPI {
private:
    SPIClass* spi;
    bool initialized;
    uint32_t currentFrequency;
    
public:
    /**
     * @brief Constructor
     * @param spiInstance Pointer to SPI instance (VSPI or HSPI)
     */
    explicit ESP32_SPI(SPIClass* spiInstance = &::SPI)
        : spi(spiInstance), initialized(false), currentFrequency(1000000) {}
    
    ~ESP32_SPI() override {
        if (initialized) {
            end();
        }
    }
    
    bool begin(uint8_t sck, uint8_t miso, uint8_t mosi, uint32_t frequency) override {
        spi->begin(sck, miso, mosi);
        currentFrequency = frequency;
        initialized = true;
        return true;
    }
    
    uint8_t transfer(uint8_t data) override {
        if (!initialized) return 0;
        return spi->transfer(data);
    }
    
    void transfer(const uint8_t* txBuffer, uint8_t* rxBuffer, size_t length) override {
        if (!initialized) return;
        
        if (rxBuffer == nullptr) {
            // Write only
            for (size_t i = 0; i < length; i++) {
                spi->transfer(txBuffer[i]);
            }
        } else {
            // Full duplex
            for (size_t i = 0; i < length; i++) {
                rxBuffer[i] = spi->transfer(txBuffer[i]);
            }
        }
    }
    
    void beginTransaction(uint8_t csPin) override {
        if (!initialized) return;
        spi->beginTransaction(SPISettings(currentFrequency, MSBFIRST, SPI_MODE0));
        digitalWrite(csPin, LOW);
    }
    
    void endTransaction(uint8_t csPin) override {
        if (!initialized) return;
        digitalWrite(csPin, HIGH);
        spi->endTransaction();
    }
    
    void end() override {
        if (initialized) {
            spi->end();
            initialized = false;
        }
    }
};

} // namespace HAL

#endif // ESP32_SPI_H