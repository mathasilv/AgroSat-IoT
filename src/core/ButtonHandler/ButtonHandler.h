/**
 * @file ButtonHandler.h
 * @brief Gerenciador de botão físico com debounce e detecção de long press
 * 
 * @details Implementa leitura de botão com:
 *          - Debounce por software (50ms)
 *          - Detecção de pressão curta (< 2s)
 *          - Detecção de pressão longa (≥ 2s)
 *          - Proteção contra disparos repetidos
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Eventos de Botão
 * | Evento      | Condição           | Uso Típico           |
 * |-------------|--------------------|-----------------------|
 * | SHORT_PRESS | Soltar antes de 2s | Alternar modo/status  |
 * | LONG_PRESS  | Manter por 2s+     | Reset/Safe mode       |
 * 
 * ## Uso
 * @code{.cpp}
 * ButtonHandler button;
 * button.begin();
 * 
 * // No loop:
 * ButtonEvent evt = button.update();
 * if (evt == ButtonEvent::LONG_PRESS) {
 *     enterSafeMode();
 * }
 * @endcode
 * 
 * @note Pino configurado em BUTTON_PIN (config.h)
 * @note Usa INPUT_PULLUP (botão conecta ao GND)
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

/**
 * @enum ButtonEvent
 * @brief Tipos de eventos de botão detectados
 */
enum class ButtonEvent : uint8_t { 
    NONE = 0,       ///< Nenhum evento
    SHORT_PRESS,    ///< Pressão curta (< 2s)
    LONG_PRESS      ///< Pressão longa (≥ 2s)
};

/**
 * @class ButtonHandler
 * @brief Gerenciador de botão com debounce e long press
 */
class ButtonHandler {
public:
    /**
     * @brief Construtor padrão
     */
    ButtonHandler();
    
    /**
     * @brief Inicializa o pino do botão
     * @note Configura como INPUT_PULLUP
     */
    void begin();
    
    /**
     * @brief Atualiza estado e detecta eventos
     * @return ButtonEvent tipo de evento detectado
     * @note Chamar frequentemente no loop (< 50ms)
     */
    ButtonEvent update();

private:
    uint8_t _pin;                    ///< Pino GPIO do botão
    bool _lastReading;               ///< Última leitura estável
    unsigned long _lastDebounceTime; ///< Timestamp do último debounce
    unsigned long _pressStartTime;   ///< Timestamp início da pressão
    bool _longPressHandled;          ///< Flag para evitar repetição

    static constexpr unsigned long DEBOUNCE_MS = 50;      ///< Tempo de debounce
    static constexpr unsigned long LONG_PRESS_MS = 2000;  ///< Tempo para long press
};

#endif // BUTTON_HANDLER_H