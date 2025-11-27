#pragma once

#include <Arduino.h>

/**
 * @file gpio_manager.h
 * @brief HAL para controle GPIO no AgroSat-IoT (Buttons + LEDs)
 * @version 1.0.0
 */

class GPIOManager {
public:
    static GPIOManager& getInstance();
    
    // Configuração de pinos
    bool setupPin(uint8_t pin, uint8_t mode);
    bool setupInput(uint8_t pin, bool pullup = true);
    bool setupOutput(uint8_t pin, bool initialState = false);
    
    // Leitura/Escrita
    bool readDigital(uint8_t pin);
    int readAnalog(uint8_t pin);
    void writeDigital(uint8_t pin, bool value);
    void writeAnalog(uint8_t pin, int value);
    
    // Controle PWM
    bool setupPWM(uint8_t pin, uint32_t freq = 1000, uint8_t resolution = 8);
    void setPWM(uint8_t pin, float dutyCycle);
    
    // Debounce para botões
    bool readDebounced(uint8_t pin, uint32_t debounceTime = BUTTON_DEBOUNCE_TIME);
    
    // Status
    bool isPinConfigured(uint8_t pin);
    
private:
    GPIOManager();
    ~GPIOManager();
    
    GPIOManager(const GPIOManager&) = delete;
    GPIOManager& operator=(const GPIOManager&) = delete;
    
    bool _pinsConfigured[48];  // GPIO 0-47
    unsigned long _lastDebounce[48];
    bool _lastState[48];
};