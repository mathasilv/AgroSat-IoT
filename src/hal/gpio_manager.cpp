#include "hal/gpio_manager.h"
#include "config.h"

GPIOManager::GPIOManager() {
    memset(_pinsConfigured, 0, sizeof(_pinsConfigured));
    memset(_lastDebounce, 0, sizeof(_lastDebounce));
    memset(_lastState, 0, sizeof(_lastState));
}

GPIOManager::~GPIOManager() {}

GPIOManager& GPIOManager::getInstance() {
    static GPIOManager instance;
    return instance;
}

bool GPIOManager::setupPin(uint8_t pin, uint8_t mode) {
    if (pin >= 48) return false;
    
    pinMode(pin, mode);
    _pinsConfigured[pin] = true;
    return true;
}

bool GPIOManager::setupInput(uint8_t pin, bool pullup) {
    if (pin >= 48) return false;
    
    pinMode(pullup ? INPUT_PULLUP : INPUT, pin);
    _pinsConfigured[pin] = true;
    return true;
}

bool GPIOManager::setupOutput(uint8_t pin, bool initialState) {
    if (pin >= 48) return false;
    
    pinMode(pin, OUTPUT);
    digitalWrite(pin, initialState);
    _pinsConfigured[pin] = true;
    return true;
}

bool GPIOManager::readDigital(uint8_t pin) {
    return digitalRead(pin);
}

int GPIOManager::readAnalog(uint8_t pin) {
    return analogRead(pin);
}

void GPIOManager::writeDigital(uint8_t pin, bool value) {
    digitalWrite(pin, value);
}

void GPIOManager::writeAnalog(uint8_t pin, int value) {
    analogWrite(pin, value);
}

bool GPIOManager::readDebounced(uint8_t pin, uint32_t debounceTime) {
    if (pin >= 48) return false;
    
    bool reading = readDigital(pin);
    unsigned long now = millis();
    
    if (reading != _lastState[pin]) {
        _lastDebounce[pin] = now;
    }
    
    if ((now - _lastDebounce[pin]) > debounceTime) {
        _lastState[pin] = reading;
    }
    
    return _lastState[pin];
}

bool GPIOManager::setupPWM(uint8_t pin, uint32_t freq, uint8_t resolution) {
    if (pin >= 48) return false;
    
    ledcSetup(0, freq, resolution);
    ledcAttachPin(pin, 0);
    _pinsConfigured[pin] = true;
    return true;
}

void GPIOManager::setPWM(uint8_t pin, float dutyCycle) {
    uint32_t duty = (1 << 8) * dutyCycle;
    ledcWrite(0, duty);
}

bool GPIOManager::isPinConfigured(uint8_t pin) {
    return pin < 48 && _pinsConfigured[pin];
}