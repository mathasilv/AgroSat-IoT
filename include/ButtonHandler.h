/**
 * @file ButtonHandler.h
 * @brief Gerenciador de botão para transição de modos PREFLIGHT ↔ FLIGHT
 * @version 1.0.0
 * @date 2025-11-24
 */
#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

enum class ButtonEvent : uint8_t {
    NONE = 0,
    SHORT_PRESS = 1,
    LONG_PRESS = 2
};

class ButtonHandler {
public:
    ButtonHandler();
    
    void begin();
    ButtonEvent update();
    
    bool isPressed() const { return _buttonState; }
    unsigned long getPressedTime() const { return _pressedTime; }
    
private:
    uint8_t _pin;
    bool _buttonState;
    bool _lastButtonState;
    unsigned long _lastDebounceTime;
    unsigned long _buttonPressTime;
    unsigned long _pressedTime;
    bool _longPressDetected;
    
    static constexpr uint32_t DEBOUNCE_DELAY = BUTTON_DEBOUNCE_TIME;
    static constexpr uint32_t LONG_PRESS_THRESHOLD = BUTTON_LONG_PRESS_TIME;
};

#endif // BUTTON_HANDLER_H
