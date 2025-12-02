/**
 * @file PowerManager.cpp
 * @brief Implementação PowerManager
 */

#include "PowerManager.h"
#include "esp_pm.h"

PowerManager::PowerManager() :
    _voltage(0.0f), _percentage(0.0f),
    _isCritical(false), _isLow(false), _powerSaveEnabled(false),
    _avgVoltage(0.0f), _lastUpdate(0)
{}

bool PowerManager::begin() {
    DEBUG_PRINTLN("[PowerManager] Inicializando...");
    pinMode(BATTERY_PIN, INPUT);
    analogReadResolution(12);
    
    // Leitura inicial rápida
    _voltage = _readVoltage();
    _avgVoltage = _voltage;
    _percentage = _calculatePercentage(_voltage);
    _updateStatus(_voltage);

    DEBUG_PRINTF("[PowerManager] Bateria: %.2fV (%.1f%%)\n", _voltage, _percentage);
    return true;
}

void PowerManager::update() {
    uint32_t now = millis();
    if (now - _lastUpdate < UPDATE_INTERVAL) return;
    _lastUpdate = now;

    float rawV = _readVoltage();
    
    // Filtro Exponencial (Low Pass) para suavizar ruído
    // Novo = 0.2 * Leitura + 0.8 * Antigo
    _avgVoltage = (0.2f * rawV) + (0.8f * _avgVoltage);
    
    _voltage = _avgVoltage;
    _percentage = _calculatePercentage(_voltage);
    
    _updateStatus(_voltage);
}

float PowerManager::_readVoltage() {
    uint32_t sum = 0;
    const int samples = 10;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(BATTERY_PIN);
        delay(2);
    }
    
    // Conversão ADC ESP32 (0-4095 -> 0-3.3V)
    // Tensão Bateria = Leitura * (3.3 / 4095) * Divisor
    // BATTERY_DIVIDER geralmente é 2.0 (Resistores iguais 100k)
    // BATTERY_VREF ajusta a referência (padrão 3.3V ou 3.6V conforme sua calibração no config.h)
    
    float rawVoltage = (sum / (float)samples) / 4095.0f * BATTERY_VREF;
    return rawVoltage * BATTERY_DIVIDER;
}

float PowerManager::_calculatePercentage(float voltage) {
    if (voltage >= BATTERY_MAX_VOLTAGE) return 100.0f;
    if (voltage <= BATTERY_MIN_VOLTAGE) return 0.0f;
    
    return (voltage - BATTERY_MIN_VOLTAGE) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE) * 100.0f;
}

void PowerManager::_updateStatus(float voltage) {
    // Lógica de Histerese para Crítico
    if (voltage < BATTERY_CRITICAL) {
        if (!_isCritical) DEBUG_PRINTLN("[PowerManager] ⚠ Bateria CRÍTICA!");
        _isCritical = true;
    } else if (voltage > (BATTERY_CRITICAL + HYSTERESIS)) {
        _isCritical = false;
    }

    // Lógica de Histerese para Baixo
    if (voltage < BATTERY_LOW) {
        _isLow = true;
    } else if (voltage > (BATTERY_LOW + HYSTERESIS)) {
        _isLow = false;
    }
}

void PowerManager::enablePowerSave() {
    if (_powerSaveEnabled) return;
    _powerSaveEnabled = true;
    
    // Reduz clock da CPU para economizar (80MHz)
    setCpuFrequencyMhz(80);
    DEBUG_PRINTLN("[PowerManager] Modo Economia ATIVADO (80MHz)");
}

void PowerManager::disablePowerSave() {
    if (!_powerSaveEnabled) return;
    _powerSaveEnabled = false;
    
    setCpuFrequencyMhz(240);
    DEBUG_PRINTLN("[PowerManager] Modo Performance ATIVADO (240MHz)");
}