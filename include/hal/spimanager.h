#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <cstdint>

#ifdef CONFIG_FREERTOS_UNICORE
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

/**
 * @brief Gerenciador singleton para SPI
 * @details Suporte LoRa SX127x + Display ST7735, thread-safe
 */
class SPIManager {
private:
    static SPIManager* instance;
    SPIManager();
    
    bool initialized = false;
    static constexpr uint32_t SPI_TIMEOUT_MS = 100;
    
#ifdef CONFIG_FREERTOS_UNICORE
    SemaphoreHandle_t mutex = nullptr;
#endif
    
public:
    static SPIManager& getInstance();
    
    SPIManager(const SPIManager&) = delete;
    SPIManager& operator=(const SPIManager&) = delete;
    SPIManager(SPIManager&&) = delete;
    SPIManager& operator=(SPIManager&&) = delete;
    
    bool init(int8_t mosi = 23, int8_t miso = 19, int8_t sck = 18, uint32_t freq = 10000000);
    
    bool transfer(uint8_t cs_pin, uint8_t* tx_buf, uint8_t* rx_buf, size_t len);
    bool transfer(uint8_t cs_pin, const uint8_t* tx_buf, size_t len);
    
    void setFrequency(uint32_t freq);
    void setChipSelect(uint8_t cs_pin, bool state);
    
    bool isInitialized() const { return initialized; }
    
    ~SPIManager();
};

inline SPIManager* SPIManager::instance = nullptr;