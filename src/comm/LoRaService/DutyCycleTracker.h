/**
 * @file DutyCycleTracker.h
 * @brief Rastreador de duty cycle para conformidade regulatória LoRa
 * 
 * @details Implementa controle de duty cycle para banda ISM 915MHz:
 *          - Janela deslizante de 1 hora
 *          - Limite conservador de 10% de tempo de transmissão
 *          - Cálculo de time-on-air por pacote
 *          - Verificação de dwell time (máx 400ms por TX)
 * 
 * @author AgroSat Team
 * @date 2025
 * @version 1.2.0
 * 
 * @copyright Copyright (c) 2025 AgroSat Project
 * @license MIT License
 * 
 * ## Regulamentação (ANATEL - Brasil, Ato 14448/2017)
 * | Parâmetro     | Limite ANATEL | Este Projeto |
 * |---------------|---------------|--------------|
 * | Duty Cycle    | Sem limite*   | 10% (conservador) |
 * | Dwell Time    | 400 ms/TX     | ~100ms (OK)  |
 * | Potência EIRP | 30 dBm        | 20 dBm       |
 * 
 * *Para LoRa (modulação digital de sequência direta), a ANATEL não
 *  impõe limite de duty cycle na faixa 915-928 MHz (plano AU915).
 *  O limite de 10% é uma escolha conservadora de projeto.
 * 
 * ## Cálculo de Duty Cycle
 * ```
 * Janela: 1 hora (3600000 ms)
 * Limite: 10% = 360000 ms de TX por hora
 * 
 * duty_cycle = (tempo_tx_acumulado / janela) * 100
 * ```
 * 
 * ## Uso
 * @code{.cpp}
 * DutyCycleTracker dc;
 * 
 * // Antes de transmitir:
 * uint32_t airTime = calculateTimeOnAir(payloadSize, SF);
 * if (dc.canTransmit(airTime)) {
 *     lora.send(data, len);
 *     dc.recordTransmission(airTime);
 * }
 * @endcode
 * 
 * @note Janela reseta automaticamente após 1 hora
 * @warning Exceder duty cycle pode resultar em multas regulatórias
 */

#ifndef DUTY_CYCLE_TRACKER_H
#define DUTY_CYCLE_TRACKER_H

#include <Arduino.h>
#include "config.h"

/**
 * @class DutyCycleTracker
 * @brief Controlador de duty cycle com janela deslizante
 */
class DutyCycleTracker {
public:
    /**
     * @brief Construtor padrão
     */
    DutyCycleTracker();
    
    //=========================================================================
    // CONTROLE DE TRANSMISSÃO
    //=========================================================================
    
    /**
     * @brief Verifica se pode transmitir sem exceder duty cycle
     * @param transmissionTimeMs Tempo estimado de TX (time-on-air) em ms
     * @return true se TX permitida dentro do limite
     * @return false se excederia o duty cycle
     */
    bool canTransmit(uint32_t transmissionTimeMs);
    
    /**
     * @brief Registra uma transmissão realizada
     * @param transmissionTimeMs Tempo real de TX em ms
     * @note Chamar APÓS cada transmissão bem sucedida
     */
    void recordTransmission(uint32_t transmissionTimeMs);
    
    //=========================================================================
    // GETTERS DE STATUS
    //=========================================================================
    
    /** @brief Retorna tempo de TX acumulado na janela atual (ms) */
    uint32_t getAccumulatedTxTime() const { return _accumulatedTxTime; }
    
    /** @brief Retorna percentual de duty cycle usado (0.0 - 100.0) */
    float getDutyCyclePercent() const;
    
private:
    //=========================================================================
    // ESTADO
    //=========================================================================
    uint32_t _windowStartTime;        ///< Início da janela atual (millis)
    uint32_t _accumulatedTxTime;      ///< TX acumulado na janela (ms)
    uint32_t _lastTransmissionTime;   ///< Timestamp última TX
    
    //=========================================================================
    // CONSTANTES (de config.h)
    //=========================================================================
    static constexpr uint32_t WINDOW_DURATION_MS = LORA_DUTY_CYCLE_WINDOW_MS;  ///< 1 hora
    static constexpr uint8_t DUTY_CYCLE_PERCENT = LORA_DUTY_CYCLE_PERCENT;     ///< 10%
    static constexpr uint32_t MAX_TX_TIME_MS = (WINDOW_DURATION_MS * DUTY_CYCLE_PERCENT) / 100;  ///< 360s
    
    /** @brief Reseta contadores se janela expirou */
    void _resetWindowIfExpired();
};

#endif