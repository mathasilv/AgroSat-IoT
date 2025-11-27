#include "hal/adc_manager.h"
#include "config.h"

ADCHelper::ADCHelper() 
    : _calibrated(false), _attenuation(ADC_11db), _resolution(12), _vref(3.3f) {
}

ADCHelper::~ADCHelper() {}

ADCHelper& ADCHelper::getInstance() {
    static ADCHelper instance;
    return instance;
}

bool ADCHelper::begin() {
    analogReadResolution(_resolution);
    analogSetAttenuation(_attenuation);
    return true;
}

void ADCHelper::calibrate() {
    // Calibração simples do VREF
    _vref = 3.3f; // ESP32 padrão
    _calibrated = true;
}

float ADCHelper::readVoltage(uint8_t pin, uint8_t samples) {
    if (samples == 0) samples = 1;
    
    float sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += analogRead(pin);
        delayMicroseconds(100);
    }
    
    return (sum / samples) * (_vref / (1 << _resolution));
}

float ADCHelper::readBattery(uint8_t pin) {
    float voltage = readVoltage(pin, BATTERY_SAMPLES);
    return voltage * BATTERY_DIVIDER;
}

uint16_t ADCHelper::readRaw(uint8_t pin) {
    return analogRead(pin);
}

void ADCHelper::setAttenuation(uint8_t attenuation) {
    _attenuation = attenuation;
    analogSetAttenuation(_attenuation);
}

void ADCHelper::setResolution(uint8_t bits) {
    _resolution = bits;
    analogReadResolution(_resolution);
}

bool ADCHelper::isCalibrated() {
    return _calibrated;
}