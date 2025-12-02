/**
 * @file ButtonHandler.cpp
 * @brief Implementação do gerenciador de botão com debounce e long-press
 */
#include "ButtonHandler.h"

ButtonHandler::ButtonHandler()
    : _pin(BUTTON_PIN)
    , _buttonState(false)
    , _lastButtonState(false)
    , _lastDebounceTime(0)
    , _buttonPressTime(0)
    , _pressedTime(0)
    , _longPressDetected(false)
{
}

void ButtonHandler::begin() {
    pinMode(_pin, INPUT_PULLUP);
    _lastButtonState = digitalRead(_pin);
    
    DEBUG_PRINTF("[ButtonHandler] Inicializado no GPIO %d\n", _pin);
    DEBUG_PRINTLN("[ButtonHandler] Funções:");
    DEBUG_PRINTLN("[ButtonHandler]   - Pressionar curto: Alternar PREFLIGHT ↔ FLIGHT");
    DEBUG_PRINTLN("[ButtonHandler]   - Pressionar 3s: Forçar SAFE MODE");
}

ButtonEvent ButtonHandler::update() {
    bool reading = digitalRead(_pin) == LOW; // Botão ativo em LOW
    ButtonEvent event = ButtonEvent::NONE;
    
    // ========== DEBOUNCE ==========
    if (reading != _lastButtonState) {
        _lastDebounceTime = millis();
    }
    
    if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
        // Estado estável após debounce
        
        if (reading != _buttonState) {
            _buttonState = reading;
            
            // ========== BOTÃO PRESSIONADO ==========
            if (_buttonState) {
                _buttonPressTime = millis();
                _longPressDetected = false;
                _pressedTime = 0;
                DEBUG_PRINTLN("[ButtonHandler] Botão pressionado");
            }
            // ========== BOTÃO LIBERADO ==========
            else {
                unsigned long pressDuration = millis() - _buttonPressTime;
                
                if (!_longPressDetected) {
                    // Short press detectado
                    if (pressDuration < LONG_PRESS_THRESHOLD) {
                        event = ButtonEvent::SHORT_PRESS;
                        DEBUG_PRINTF("[ButtonHandler] Short press detectado (%lu ms)\n", pressDuration);
                    }
                }
                
                _pressedTime = 0;
                _buttonPressTime = 0;
            }
        }
        
        // ========== DETECÇÃO DE LONG PRESS (ENQUANTO PRESSIONADO) ==========
        if (_buttonState && !_longPressDetected) {
            _pressedTime = millis() - _buttonPressTime;
            
            if (_pressedTime >= LONG_PRESS_THRESHOLD) {
                _longPressDetected = true;
                event = ButtonEvent::LONG_PRESS;
                DEBUG_PRINTF("[ButtonHandler] Long press detectado (%lu ms)\n", _pressedTime);
            }
        }
    }
    
    _lastButtonState = reading;
    return event;
}
