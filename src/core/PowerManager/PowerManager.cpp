/**
 * @file PowerManager.cpp
 * @brief Implementação PowerManager com Curva Li-ion Real
 * @version 2.0.0
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
    
    // Filtro Exponencial (Low Pass)
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
    
    float rawVoltage = (sum / (float)samples) / 4095.0f * BATTERY_VREF;
    return rawVoltage * BATTERY_DIVIDER;
}

// ============================================================================
// CORRIGIDO 4.4: Curva de Descarga Li-ion 18650 Real (Não-Linear)
// ============================================================================
float PowerManager::_calculatePercentage(float v) {
    // Curva baseada em testes reais de baterias Li-ion 18650 (3.7V nominal)
    // Fonte: Battery University + Testes empíricos
    
    if (v >= 4.20f) return 100.0f;
    if (v >= 4.15f) return 95.0f + (v - 4.15f) * 100.0f;   // 4.15-4.20V: 95-100%
    if (v >= 4.10f) return 90.0f + (v - 4.10f) * 100.0f;   // 4.10-4.15V: 90-95%
    if (v >= 4.00f) return 80.0f + (v - 4.00f) * 100.0f;   // 4.00-4.10V: 80-90%
    if (v >= 3.90f) return 65.0f + (v - 3.90f) * 150.0f;   // 3.90-4.00V: 65-80%
    if (v >= 3.80f) return 45.0f + (v - 3.80f) * 200.0f;   // 3.80-3.90V: 45-65%
    if (v >= 3.70f) return 25.0f + (v - 3.70f) * 200.0f;   // 3.70-3.80V: 25-45%
    if (v >= 3.60f) return 10.0f + (v - 3.60f) * 150.0f;   // 3.60-3.70V: 10-25%
    if (v >= 3.50f) return 5.0f  + (v - 3.50f) * 50.0f;    // 3.50-3.60V: 5-10%
    if (v >= 3.40f) return 2.0f  + (v - 3.40f) * 30.0f;    // 3.40-3.50V: 2-5%
    if (v >= 3.30f) return 0.5f  + (v - 3.30f) * 15.0f;    // 3.30-3.40V: 0.5-2%
    
    return 0.0f;  // < 3.30V: Crítico
}

void PowerManager::_updateStatus(float voltage) {
    // Histerese para Crítico
    if (voltage < BATTERY_CRITICAL) {
        if (!_isCritical) DEBUG_PRINTLN("[PowerManager] ⚠ Bateria CRÍTICA!");
        _isCritical = true;
    } else if (voltage > (BATTERY_CRITICAL + HYSTERESIS)) {
        _isCritical = false;
    }

    // Histerese para Baixo
    if (voltage < BATTERY_LOW) {
        _isLow = true;
    } else if (voltage > (BATTERY_LOW + HYSTERESIS)) {
        _isLow = false;
    }
}

void PowerManager::enablePowerSave() {
    if (_powerSaveEnabled) return;
    _powerSaveEnabled = true;
    
    setCpuFrequencyMhz(80);
    DEBUG_PRINTLN("[PowerManager] Modo Economia ATIVADO (80MHz)");
}

void PowerManager::disablePowerSave() {
    if (!_powerSaveEnabled) return;
    _powerSaveEnabled = false;
    
    setCpuFrequencyMhz(240);
    DEBUG_PRINTLN("[PowerManager] Modo Performance ATIVADO (240MHz)");
}

// ============================================================================
// NOVO 5.2: Ajuste Dinâmico de CPU Baseado em Carga
// ============================================================================
void PowerManager::adjustCpuFrequency() {
    if (_percentage > 60.0f) {
        if (getCpuFrequencyMhz() != 240) {
            setCpuFrequencyMhz(240);  // Performance
            DEBUG_PRINTLN("[PowerManager] CPU: 240MHz (Performance)");
        }
    } else if (_percentage > 30.0f) {
        if (getCpuFrequencyMhz() != 160) {
            setCpuFrequencyMhz(160);  // Balanced
            DEBUG_PRINTLN("[PowerManager] CPU: 160MHz (Balanced)");
        }
    } else if (_percentage > 15.0f) {
        if (getCpuFrequencyMhz() != 80) {
            setCpuFrequencyMhz(80);   // Economy
            DEBUG_PRINTLN("[PowerManager] CPU: 80MHz (Economy)");
        }
    } else {
        // Crítico: Força economia máxima
        if (getCpuFrequencyMhz() != 80) {
            setCpuFrequencyMhz(80);
            DEBUG_PRINTLN("[PowerManager] CPU: 80MHz (CRÍTICO)");
        }
    }
}