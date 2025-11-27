#pragma once
#include <Arduino.h>
#include <cstdint>

/**
 * @brief Gerenciador ADC com calibração e averaging
 */
class ADCManager {
private:
    static ADCManager* instance;
    ADCManager() = default;
    
public:
    static ADCManager& getInstance();
    
    ADCManager(const ADCManager&) = delete;
    ADCManager& operator=(const ADCManager&) = delete;
    
    bool init();
    
    float analogRead(uint8_t pin, uint16_t samples = 16);
    float analogReadVoltage(uint8_t pin, uint16_t samples = 16);
    
    void setAttenuation(adc_attenuation_t atten);
    
    bool isInitialized() const { return true; }
};

inline ADCManager* ADCManager::instance = nullptr;