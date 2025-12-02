#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

enum class ButtonEvent : uint8_t { NONE = 0, SHORT_PRESS, LONG_PRESS };

class ButtonHandler {
public:
    ButtonHandler();
    void begin();
    ButtonEvent update();

private:
    uint8_t _pin;
    bool _lastReading;
    unsigned long _lastDebounceTime;
    unsigned long _pressStartTime;
    bool _longPressHandled; // Evita disparar long press repetidamente

    static constexpr unsigned long DEBOUNCE_MS = 50;
    static constexpr unsigned long LONG_PRESS_MS = 2000; // 2 segundos
};

#endif