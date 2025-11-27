#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <cstdint>

#ifdef CONFIG_FREERTOS_UNICORE
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

/**
 * @brief Gerenciador singleton para I2C
 * @details Thread-safe, inicialização lazy, timeout handling
 */
class I2CManager {
private:
    // Singleton
    static I2CManager* instance;
    I2CManager();
    
    // Estado interno
    bool initialized = false;
    TwoWire* wire = nullptr;
    static constexpr uint32_t I2C_TIMEOUT_MS = 100;
    
#ifdef CONFIG_FREERTOS_UNICORE
    SemaphoreHandle_t mutex = nullptr;
#endif
    
public:
    static I2CManager& getInstance();
    
    // Delete copy/move constructors
    I2CManager(const I2CManager&) = delete;
    I2CManager& operator=(const I2CManager&) = delete;
    I2CManager(I2CManager&&) = delete;
    I2CManager& operator=(I2CManager&&) = delete;
    
    bool init(uint8_t sda = 21, uint8_t scl = 22, uint32_t freq = 400000);
    
    bool write(uint8_t addr, uint8_t reg, uint8_t data);
    bool write(uint8_t addr, uint8_t reg, const uint8_t* data, size_t len);
    bool read(uint8_t addr, uint8_t reg, uint8_t* data, size_t len);
    
    bool probe(uint8_t addr);
    void scan();
    void reset();
    
    bool isInitialized() const { return initialized; }
    
    ~I2CManager();
};

inline I2CManager* I2CManager::instance = nullptr;