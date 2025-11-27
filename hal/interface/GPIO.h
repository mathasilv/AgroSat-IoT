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

enum class PinMode {
    INPUT,
    OUTPUT,
    INPUT_PULLUP,
    INPUT_PULLDOWN
};

enum class PinState {
    LOW = 0,
    HIGH = 1
};

/**
 * @class GPIO
 * @brief Abstract base class for GPIO hardware abstraction
 */
class GPIO {
public:
    virtual ~GPIO() = default;
    
    /**
     * @brief Set pin mode
     * @param pin Pin number
     * @param mode Pin mode (INPUT, OUTPUT, etc.)
     */
    virtual void pinMode(uint8_t pin, PinMode mode) = 0;
    
    /**
     * @brief Write digital value to pin
     * @param pin Pin number
     * @param state Pin state (LOW or HIGH)
     */
    virtual void digitalWrite(uint8_t pin, PinState state) = 0;
    
    /**
     * @brief Read digital value from pin
     * @param pin Pin number
     * @return Current pin state
     */
    virtual PinState digitalRead(uint8_t pin) = 0;
    
    /**
     * @brief Attach interrupt to pin
     * @param pin Pin number
     * @param callback Interrupt callback function
     * @param mode Interrupt mode (RISING, FALLING, CHANGE)
     */
    virtual void attachInterrupt(uint8_t pin, void (*callback)(), int mode) = 0;
    
    /**
     * @brief Detach interrupt from pin
     * @param pin Pin number
     */
    virtual void detachInterrupt(uint8_t pin) = 0;
};

} // namespace HAL

#endif // HAL_GPIO_H