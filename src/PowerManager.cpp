/**
 * @file PowerManager.cpp
 * @brief Implementação usando HAL::adc()
 * @version 2.0.0
 */

#include "PowerManager.h"

PowerManager::PowerManager() :
    _voltage(0.0),
    _percentage(0.0),
    _avgVoltage(0.0),
    _minVoltage(999.9),
    _maxVoltage(0.0),
    _lastReadTime(0),
    _sampleCount(0),
    _powerSaveEnabled(false)
{
}

bool PowerManager::begin() {
    DEBUG_PRINTLN("[PowerManager] Inicializando com HAL ADC...");
    
    // ✅ MIGRAÇÃO: HAL ADC já configura tudo
    HAL::adc().begin();
    HAL::adc().calibrate();
    
    // Primeira leitura
    _voltage = HAL::adc().readBattery(BATTERY_PIN);
    _percentage = _voltageToPercentage(_voltage);
    
    DEBUG_PRINTF("[PowerManager] Tensão inicial: %.2fV (%.1f%%)\n", 
                 _voltage, _percentage);
    
    return true;
}

void PowerManager::update() {
    uint32_t currentTime = millis();
    
    if (currentTime - _lastReadTime >= 1000) {
        _lastReadTime = currentTime;
        
        // ✅ MIGRAÇÃO: HAL::adc().readBattery() já faz média
        float newVoltage = HAL::adc().readBattery(BATTERY_PIN);
        
        _sampleCount++;
        _avgVoltage = (_avgVoltage * (_sampleCount - 1) + newVoltage) / _sampleCount;
        
        if (newVoltage < _minVoltage) _minVoltage = newVoltage;
        if (newVoltage > _maxVoltage) _maxVoltage = newVoltage;
        
        _voltage = newVoltage;
        _percentage = _voltageToPercentage(_voltage);
        
        if (isCritical()) {
            DEBUG_PRINTLN("[PowerManager] ALERTA: Bateria CRÍTICA!");
        } else if (isLow()) {
            DEBUG_PRINTLN("[PowerManager] AVISO: Bateria BAIXA");
        }
    }
}

float PowerManager::getVoltage() { return _voltage; }
float PowerManager::getPercentage() { return _percentage; }
float PowerManager::getCurrent() { return 0.0; }
float PowerManager::getPower() { return _voltage * getCurrent(); }

uint16_t PowerManager::getTimeRemaining() {
    float batteryCapacity = 2000.0;  // mAh
    float avgCurrent = 100.0;        // mA
    
    if (avgCurrent <= 0) return 0;
    
    float remainingCapacity = (batteryCapacity * _percentage) / 100.0;
    float timeHours = remainingCapacity / avgCurrent;
    
    return (uint16_t)(timeHours * 60);
}

bool PowerManager::isCritical() { return _voltage < BATTERY_CRITICAL; }
bool PowerManager::isLow() { return _voltage < BATTERY_LOW; }

void PowerManager::enablePowerSave() {
    _powerSaveEnabled = true;
    setCpuFrequencyMhz(80);
    DEBUG_PRINTLN("[PowerManager] Economia ativada (80 MHz)");
}

void PowerManager::disablePowerSave() {
    _powerSaveEnabled = false;
    setCpuFrequencyMhz(240);
    DEBUG_PRINTLN("[PowerManager] Economia desativada (240 MHz)");
}

void PowerManager::deepSleep(uint64_t durationSeconds) {
    DEBUG_PRINTF("[PowerManager] Deep sleep %llu s\n", durationSeconds);
    esp_sleep_enable_timer_wakeup(durationSeconds * 1000000ULL);
    esp_deep_sleep_start();
}

void PowerManager::getStatistics(float& avgVoltage, float& minVoltage, float& maxVoltage) {
    avgVoltage = _avgVoltage;
    minVoltage = _minVoltage;
    maxVoltage = _maxVoltage;
}

float PowerManager::_readVoltage() {
    // ✅ REMOVIDO: HAL::adc().readBattery() já faz média
    return HAL::adc().readBattery(BATTERY_PIN);
}

float PowerManager::_voltageToPercentage(float voltage) {
    if (voltage >= BATTERY_MAX_VOLTAGE) return 100.0;
    if (voltage <= BATTERY_MIN_VOLTAGE) return 0.0;
    
    float range = BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE;
    return constrain(((voltage - BATTERY_MIN_VOLTAGE) / range) * 100.0, 0.0, 100.0);
}