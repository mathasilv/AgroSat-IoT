#include "../include/hal/adcmanager.h"

ADCManager& ADCManager::getInstance() {
    if (!instance) {
        instance = new ADCManager();
    }
    return *instance;
}

bool ADCManager::init() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.println("[ADC] Initialized (12-bit, 11dB attenuation)");
    return true;
}

float ADCManager::analogRead(uint8_t pin, uint16_t samples) {
    uint32_t sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(10);
    }
    return (float)sum / samples;
}

float ADCManager::analogReadVoltage(uint8_t pin, uint16_t samples) {
    return analogRead(pin, samples) * 3.3f / 4095.0f;
}

void ADCManager::setAttenuation(adc_attenuation_t atten) {
    analogSetAttenuation(atten);
}