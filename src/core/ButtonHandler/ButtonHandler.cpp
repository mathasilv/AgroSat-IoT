#include "ButtonHandler.h"

ButtonHandler::ButtonHandler() 
    : _pin(BUTTON_PIN), _lastReading(HIGH), 
      _lastDebounceTime(0), _pressStartTime(0), _longPressHandled(false) 
{}

void ButtonHandler::begin() {
    pinMode(_pin, INPUT_PULLUP);
    _lastReading = digitalRead(_pin);
    DEBUG_PRINTLN("[ButtonHandler] Inicializado.");
}

ButtonEvent ButtonHandler::update() {
    bool reading = digitalRead(_pin);
    unsigned long now = millis();
    ButtonEvent event = ButtonEvent::NONE;

    // Reset se estado mudou (ruído ou ação)
    if (reading != _lastReading) {
        _lastDebounceTime = now;
    }

    // Se estado estável por tempo suficiente
    if ((now - _lastDebounceTime) > DEBOUNCE_MS) {
        // Detectou Pressionamento (LOW no Pull-up)
        if (reading == LOW) {
            if (_pressStartTime == 0) {
                _pressStartTime = now;
                _longPressHandled = false;
            } else if (!_longPressHandled && (now - _pressStartTime > LONG_PRESS_MS)) {
                // Long Press detectado enquanto segura
                _longPressHandled = true;
                return ButtonEvent::LONG_PRESS;
            }
        } 
        // Detectou Soltura (Borda de subida)
        else if (reading == HIGH && _pressStartTime != 0) {
            if (!_longPressHandled) {
                // Soltou antes do tempo de long press -> Short Press
                event = ButtonEvent::SHORT_PRESS;
            }
            _pressStartTime = 0; // Reset
        }
    }

    _lastReading = reading;
    return event;
}