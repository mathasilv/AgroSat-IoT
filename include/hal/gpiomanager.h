#pragma once
#include <Arduino.h>
#include <cstdint>

#ifdef CONFIG_FREERTOS_UNICORE
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

/**
 * @brief Gerenciador GPIO com debouncing e interrupts
 */
class GPIOManager {
private:
    static GPIOManager* instance;
    GPIOManager() = default;
    
#ifdef CONFIG_FREERTOS_UNICORE
    SemaphoreHandle_t mutex = nullptr;
#endif
    
public:
    static GPIOManager& getInstance();
    
    GPIOManager(const GPIOManager&) = delete;
    GPIOManager& operator=(const GPIOManager&) = delete;
    
    bool init();
    
    void setPinMode(uint8_t pin, uint8_t mode);
    bool digitalRead(uint8_t pin);
    void digitalWrite(uint8_t pin, bool value);
    
    bool debounceRead(uint8_t pin, uint32_t debounce_ms = 50);
    
    void attachInterrupt(uint8_t pin, void (*ISR)(void), int mode);
    void detachInterrupt(uint8_t pin);
    
    bool isInitialized() const { return true; } // Sempre disponível
};

inline GPIOManager* GPIOManager::instance = nullptr;