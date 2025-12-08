/**
 * @file DutyCycleTracker.h
 * @brief Rastreador de Duty Cycle LoRa (ANATEL 915MHz - 10%)
 * @version 1.0.0 (MODERADO 4.8 - Rate Limiting Implementado)
 */

#ifndef DUTY_CYCLE_TRACKER_H
#define DUTY_CYCLE_TRACKER_H

#include <Arduino.h>
#include "config.h"

class DutyCycleTracker {
public:
    DutyCycleTracker();
    
    /**
     * @brief Verifica se pode transmitir (duty cycle não excedido)
     * @param transmissionTimeMs Tempo estimado da transmissão em ms
     * @return true se pode transmitir
     */
    bool canTransmit(uint32_t transmissionTimeMs);
    
    /**
     * @brief Registra uma transmissão realizada
     * @param transmissionTimeMs Tempo da transmissão em ms
     */
    void recordTransmission(uint32_t transmissionTimeMs);
    
    /**
     * @brief Retorna o tempo de TX acumulado na janela atual (ms)
     */
    uint32_t getAccumulatedTxTime() const { return _accumulatedTxTime; }
    
    /**
     * @brief Retorna o tempo restante disponível na janela (ms)
     */
    uint32_t getRemainingTime() const;
    
    /**
     * @brief Retorna o percentual de duty cycle usado (0-100)
     */
    float getDutyCyclePercent() const;
    
    /**
     * @brief Retorna tempo até poder transmitir novamente (ms)
     * @return 0 se pode transmitir agora, >0 caso contrário
     */
    uint32_t getTimeUntilAvailable(uint32_t transmissionTimeMs) const;
    
private:
    uint32_t _windowStartTime;        // Início da janela de 1 hora
    uint32_t _accumulatedTxTime;      // Tempo de TX acumulado na janela (ms)
    uint32_t _lastTransmissionTime;   // Timestamp da última TX
    
    static constexpr uint32_t WINDOW_DURATION_MS = LORA_DUTY_CYCLE_WINDOW_MS;  // 1 hora
    static constexpr uint8_t DUTY_CYCLE_PERCENT = LORA_DUTY_CYCLE_PERCENT;     // 10%
    static constexpr uint32_t MAX_TX_TIME_MS = (WINDOW_DURATION_MS * DUTY_CYCLE_PERCENT) / 100;  // 6 minutos
    
    void _resetWindowIfExpired();
};

#endif