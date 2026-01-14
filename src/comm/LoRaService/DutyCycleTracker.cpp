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
    
    if (_accumulatedTxTime + transmissionTimeMs > MAX_TX_TIME_MS) {
        DEBUG_PRINTF("[DutyCycle] Bloqueado: %lu/%lu ms usado\n",
                     _accumulatedTxTime, MAX_TX_TIME_MS);
        return false;
    }
    return true;
}

void DutyCycleTracker::recordTransmission(uint32_t transmissionTimeMs) {
    _resetWindowIfExpired();
    _accumulatedTxTime += transmissionTimeMs;
    _lastTransmissionTime = millis();
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
        return 0;
    }
    
    uint32_t now = millis();
    uint32_t windowElapsed = now - _windowStartTime;
    
    if (windowElapsed >= WINDOW_DURATION_MS) {
        return 0;
    }
    return WINDOW_DURATION_MS - windowElapsed;
}

void DutyCycleTracker::_resetWindowIfExpired() {
    uint32_t now = millis();
    uint32_t windowElapsed = now - _windowStartTime;
    
    if (windowElapsed >= WINDOW_DURATION_MS) {
        _windowStartTime = now;
        _accumulatedTxTime = 0;
    }
}