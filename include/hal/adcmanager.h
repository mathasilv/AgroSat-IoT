#pragma once
#include <Arduino.h>
#include <cstdint>
#include <driver/adc.h>

/**
 * @brief Gerenciador ADC com calibração ESP32
 */
class ADCManager {
private:
    static ADCManager* instance;
    ADCManager();
    
public:
    static ADCManager& getInstance();
    
    ADCManager(const ADCManager&) = delete;
    ADCManager& operator=(const ADCManager&) = delete;
    
    bool init();
    
    float analogRead(uint8_t pin, uint16_t samples = 16);
    float analogReadVoltage(uint8_t pin, uint16_t samples = 16);
    
    void setAttenuation(adc_attenuation_t atten);
    
    bool isInitialized() const;
    
    ~ADCManager();
};

inline ADCManager* ADCManager::instance = nullptr;