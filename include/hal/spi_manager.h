#pragma once

#include <Arduino.h>
#include <SPI.h>

/**
 * @file spi_manager.h
 * @brief HAL para comunicação SPI no AgroSat-IoT (LoRa + SD Card)
 * @version 1.0.0
 */

class SPIManager {
public:
    static SPIManager& getInstance();
    
    bool begin();
    void end();
    
    // Transações SPI
    bool transfer(uint8_t csPin, uint8_t* txData, uint8_t* rxData, size_t length);
    bool transferBytes(uint8_t csPin, uint8_t* data, size_t length);
    
    // Transações comuns
    uint8_t transferByte(uint8_t csPin, uint8_t data);
    bool writeByte(uint8_t csPin, uint8_t data);
    uint8_t readByte(uint8_t csPin);
    
    // Controle de pinos CS
    void select(uint8_t csPin);
    void deselect(uint8_t csPin);
    
    // Configurações
    void setFrequency(uint32_t freq);
    void setDataMode(uint8_t mode);
    void setBitOrder(uint8_t order);
    
    bool isReady();
    
private:
    SPIManager();
    ~SPIManager();
    
    SPIManager(const SPIManager&) = delete;
    SPIManager& operator=(const SPIManager&) = delete;
    
    SPIClass* _spi;
    uint32_t _frequency;
    uint8_t _dataMode;
    uint8_t _bitOrder;
    bool _initialized;
};