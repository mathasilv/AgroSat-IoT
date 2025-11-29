/**
 * @file ESP32_GPIO.h
 * @brief ESP32 Implementation of GPIO HAL
 * @details Concrete implementation using Arduino GPIO functions
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef ESP32_GPIO_H
#define ESP32_GPIO_H

#include <HAL/interface/GPIO.h>
#include <Arduino.h>

namespace HAL {

/**
 * @class ESP32_GPIO
 * @brief ESP32-specific GPIO implementation
 */
class ESP32_GPIO : public GPIO {
public:
    ESP32_GPIO() = default;
    ~ESP32_GPIO() override = default;
    
    void pinMode(uint8_t pin, PinMode mode) override {
        switch (mode) {
            case PinMode::Input:
                ::pinMode(pin, INPUT);
                break;
            case PinMode::Output:
                ::pinMode(pin, OUTPUT);
                break;
            case PinMode::InputPullup:
                ::pinMode(pin, INPUT_PULLUP);
                break;
            case PinMode::InputPulldown:
                ::pinMode(pin, INPUT_PULLDOWN);
                break;
        }
    }
    
    void digitalWrite(uint8_t pin, PinState state) override {
        ::digitalWrite(pin, (state == PinState::High) ? HIGH : LOW);
    }
    
    PinState digitalRead(uint8_t pin) override {
        return (::digitalRead(pin) == HIGH) ? PinState::High : PinState::Low;
    }
    
    void attachInterrupt(uint8_t pin, void (*callback)(), int mode) override {
        ::attachInterrupt(digitalPinToInterrupt(pin), callback, mode);
    }
    
    void detachInterrupt(uint8_t pin) override {
        ::detachInterrupt(digitalPinToInterrupt(pin));
    }
};

} // namespace HAL

#endif // ESP32_GPIO_H
