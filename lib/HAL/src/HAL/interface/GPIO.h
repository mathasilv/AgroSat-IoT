/**
 * @file GPIO.h
 * @brief Hardware Abstraction Layer - GPIO Interface
 * @details Abstract interface for GPIO operations
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

namespace HAL {

// IMPORTANT:
// Não usar nomes INPUT/OUTPUT/LOW/HIGH por causa dos macros do Arduino.
// Usamos nomes neutros para evitar conflito.

enum class PinMode {
    Input,
    Output,
    InputPullup,
    InputPulldown
};

enum class PinState {
    Low  = 0,
    High = 1
};

/**
 * @class GPIO
 * @brief Abstract base class for GPIO hardware abstraction
 */
class GPIO {
public:
    virtual ~GPIO() = default;
    
    /// Set pin mode
    virtual void pinMode(uint8_t pin, PinMode mode) = 0;
    
    /// Write digital value to pin
    virtual void digitalWrite(uint8_t pin, PinState state) = 0;
    
    /// Read digital value from pin
    virtual PinState digitalRead(uint8_t pin) = 0;
    
    /// Attach interrupt to pin
    virtual void attachInterrupt(uint8_t pin, void (*callback)(), int mode) = 0;
    
    /// Detach interrupt from pin
    virtual void detachInterrupt(uint8_t pin) = 0;
};

} // namespace HAL

#endif // HAL_GPIO_H
