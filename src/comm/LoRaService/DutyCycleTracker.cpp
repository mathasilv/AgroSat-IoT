/**
 * @file DutyCycleTracker.cpp
 * @brief Implementação do Rastreador de Duty Cycle LoRa
 */

#include "DutyCycleTracker.h"

DutyCycleTracker::DutyCycleTracker() :
    _windowStartTime(millis()),
    _accumulatedTxTime(0),
    _lastTransmissionTime(0)
{}

bool DutyCycleTracker::canTransmit(uint32_t transmissionTimeMs) {
    _resetWindowIfExpired();
    
    // Verifica se adicionar essa TX excederia o limite
    if (_accumulatedTxTime + transmissionTimeMs > MAX_TX_TIME_MS) {
        DEBUG_PRINTF("[DutyCycle] BLOQUEADO: TX=%lu ms excederia limite (%lu/%lu usado)\n",
                     transmissionTimeMs, _accumulatedTxTime, MAX_TX_TIME_MS);
        return false;
    }
    
    return true;
}

void DutyCycleTracker::recordTransmission(uint32_t transmissionTimeMs) {
    _resetWindowIfExpired();
    
    _accumulatedTxTime += transmissionTimeMs;
    _lastTransmissionTime = millis();
    
    DEBUG_PRINTF("[DutyCycle] TX registrado: +%lu ms (Total: %lu/%lu ms, %.1f%%)\n",
                 transmissionTimeMs, _accumulatedTxTime, MAX_TX_TIME_MS, 
                 getDutyCyclePercent());
}

uint32_t DutyCycleTracker::getRemainingTime() const {
    if (_accumulatedTxTime >= MAX_TX_TIME_MS) return 0;
    return MAX_TX_TIME_MS - _accumulatedTxTime;
}

float DutyCycleTracker::getDutyCyclePercent() const {
    return (_accumulatedTxTime / (float)MAX_TX_TIME_MS) * 100.0f;
}

uint32_t DutyCycleTracker::getTimeUntilAvailable(uint32_t transmissionTimeMs) const {
    if (_accumulatedTxTime + transmissionTimeMs <= MAX_TX_TIME_MS) {
        return 0;  // Pode transmitir agora
    }
    
    // Calcular quanto tempo falta para a janela expirar
    uint32_t now = millis();
    uint32_t windowElapsed = now - _windowStartTime;
    
    if (windowElapsed >= WINDOW_DURATION_MS) {
        return 0;  // Janela já expirou, será resetada
    }
    
    return WINDOW_DURATION_MS - windowElapsed;
}

void DutyCycleTracker::_resetWindowIfExpired() {
    uint32_t now = millis();
    uint32_t windowElapsed = now - _windowStartTime;
    
    // Se passou 1 hora, reseta a janela
    if (windowElapsed >= WINDOW_DURATION_MS) {
        DEBUG_PRINTF("[DutyCycle] Janela expirada. Resetando contadores (Usado: %.1f%%)\n", 
                     getDutyCyclePercent());
        
        _windowStartTime = now;
        _accumulatedTxTime = 0;
    }
}