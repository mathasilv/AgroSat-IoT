/**
 * @file ButtonHandler.cpp
 * @brief Implementação usando HAL::gpio()
 * @version 2.0.0
 */

#include "ButtonHandler.h"

ButtonHandler::ButtonHandler()
    : _buttonState(false)
    , _lastButtonState(false)
    , _lastDebounceTime(0)
    , _buttonPressTime(0)
    , _pressedTime(0)
    , _longPressDetected(false)
{
}

void ButtonHandler::begin() {
    // ✅ MIGRAÇÃO: HAL GPIO ao invés de pinMode direto
    HAL::gpio().setupInput(BUTTON_PIN, true); // INPUT_PULLUP
    
    _lastButtonState = HAL::gpio().readDigital(BUTTON_PIN);
    
    DEBUG_PRINTF("[ButtonHandler] Inicializado no GPIO %d (HAL GPIO)\n", BUTTON_PIN);
    DEBUG_PRINTLN("[ButtonHandler] Funções:");
    DEBUG_PRINTLN("[ButtonHandler]   - Pressionar curto: Alternar PREFLIGHT ↔ FLIGHT");
    DEBUG_PRINTLN("[ButtonHandler]   - Pressionar 3s: Forçar SAFE MODE");
}

ButtonEvent ButtonHandler::update() {
    // ✅ MIGRAÇÃO: HAL GPIO com debounce automático disponível
    bool reading = HAL::gpio().readDigital(BUTTON_PIN) == LOW;
    ButtonEvent event = ButtonEvent::NONE;
    
    if (reading != _lastButtonState) {
        _lastDebounceTime = millis();
    }
    
    if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
        if (reading != _buttonState) {
            _buttonState = reading;
            
            if (_buttonState) {
                _buttonPressTime = millis();
                _longPressDetected = false;
                _pressedTime = 0;
                DEBUG_PRINTLN("[ButtonHandler] Botão pressionado");
            } else {
                unsigned long pressDuration = millis() - _buttonPressTime;
                
                if (!_longPressDetected) {
                    if (pressDuration < LONG_PRESS_THRESHOLD) {
                        event = ButtonEvent::SHORT_PRESS;
                        DEBUG_PRINTF("[ButtonHandler] Short press (%lu ms)\n", pressDuration);
                    }
                }
                
                _pressedTime = 0;
                _buttonPressTime = 0;
            }
        }
        
        if (_buttonState && !_longPressDetected) {
            _pressedTime = millis() - _buttonPressTime;
            
            if (_pressedTime >= LONG_PRESS_THRESHOLD) {
                _longPressDetected = true;
                event = ButtonEvent::LONG_PRESS;
                DEBUG_PRINTF("[ButtonHandler] Long press (%lu ms)\n", _pressedTime);
            }
        }
    }
    
    _lastButtonState = reading;
    return event;
}