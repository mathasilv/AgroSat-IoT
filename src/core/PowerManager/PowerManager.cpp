#include "PowerManager.h"
#include <float.h>          // Para FLT_MAX
#include "esp_pm.h"         // Para setCpuFrequencyMhz

PowerManager::PowerManager() :
    _voltage(0.0f),
    _percentage(0.0f),
    _avgVoltage(0.0f),
    _minVoltage(FLT_MAX),
    _maxVoltage(0.0f),
    _lastReadTime(0),
    _sampleCount(0),
    _powerSaveEnabled(false)
{}

bool PowerManager::begin() {
    DEBUG_PRINTLN("[PowerManager] Inicializando...");
    pinMode(BATTERY_PIN, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    _voltage = _readVoltage();
    _percentage = _voltageToPercentage(_voltage);

    DEBUG_PRINTF("[PowerManager] Tensão inicial: %.2fV (%.1f%%)\n", _voltage, _percentage);
    return true;
}

void PowerManager::update() {
    uint32_t now = millis();
    if (now - _lastReadTime >= 1000) {
        _lastReadTime = now;
        float newVoltage = _readVoltage();

        _sampleCount++;
        _avgVoltage = ((_avgVoltage * (_sampleCount - 1)) + newVoltage) / _sampleCount;

        if (newVoltage < _minVoltage) _minVoltage = newVoltage;
        if (newVoltage > _maxVoltage) _maxVoltage = newVoltage;

        _voltage = newVoltage;
        _percentage = _voltageToPercentage(newVoltage);

        if (isCritical()) {
            DEBUG_PRINTLN("[PowerManager] ALERTA: Bateria em nível CRÍTICO!");
        } else if (isLow()) {
            DEBUG_PRINTLN("[PowerManager] AVISO: Bateria em nível BAIXO");
        }
    }
}

float PowerManager::_readVoltage() {
    float sum = 0.0f;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        sum += analogRead(BATTERY_PIN);
        delayMicroseconds(100);
    }
    float avgRaw = sum / BATTERY_SAMPLES;
    return (avgRaw / 4095.0f) * BATTERY_VREF * BATTERY_DIVIDER;
}

float PowerManager::_voltageToPercentage(float voltage) {
    if (voltage >= BATTERY_MAX_VOLTAGE) return 100.0f;
    if (voltage <= BATTERY_MIN_VOLTAGE) return 0.0f;

    float range = BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE;
    float percentage = ((voltage - BATTERY_MIN_VOLTAGE) / range) * 100.0f;

    return constrain(percentage, 0.0f, 100.0f);
}

float PowerManager::getVoltage() {
    return _voltage;
}

float PowerManager::getPercentage() {
    return _percentage;
}

bool PowerManager::isCritical() {
    return _voltage < BATTERY_CRITICAL;
}

bool PowerManager::isLow() {
    return _voltage < BATTERY_LOW;
}

void PowerManager::enablePowerSave() {
    _powerSaveEnabled = true;
    setCpuFrequencyMhz(80);
    DEBUG_PRINTLN("[PowerManager] Modo economia ativado (80 MHz)");
}

void PowerManager::disablePowerSave() {
    _powerSaveEnabled = false;
    setCpuFrequencyMhz(240);
    DEBUG_PRINTLN("[PowerManager] Modo economia desativado (240 MHz)");
}

void PowerManager::getStatistics(float& avgVoltage, float& minVoltage, float& maxVoltage) {
    avgVoltage = _avgVoltage;
    minVoltage = _minVoltage;
    maxVoltage = _maxVoltage;
}
