#pragma once

#include <Arduino.h>

/**
 * @file adc_manager.h
 * @brief HAL para leitura ADC no AgroSat-IoT (Bateria + Sensores Analógicos)
 * @version 1.0.0
 */

class ADCHelper {
public:
    static ADCHelper& getInstance();
    
    bool begin();
    void calibrate();
    
    // Leitura com média móvel
    float readVoltage(uint8_t pin, uint8_t samples = 16);
    float readBattery(uint8_t pin);
    uint16_t readRaw(uint8_t pin);
    
    // Configurações
    void setAttenuation(uint8_t attenuation);
    void setResolution(uint8_t bits);
    
    bool isCalibrated();
    
private:
    ADCHelper();
    ~ADCHelper();
    
    ADCHelper(const ADCHelper&) = delete;
    ADCHelper& operator=(const ADCHelper&) = delete;
    
    bool _calibrated;
    uint8_t _attenuation;
    uint8_t _resolution;
    float _vref;
};