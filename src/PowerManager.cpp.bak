/**
 * @file PowerManager.cpp
 * @brief Implementação do gerenciador de energia
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
    DEBUG_PRINTLN("[PowerManager] Inicializando...");
    
    // Configurar pino de leitura de bateria
    pinMode(BATTERY_PIN, INPUT);
    
    // Configurar ADC
    analogReadResolution(12);  // 12 bits (0-4095)
    analogSetAttenuation(ADC_11db);  // 0-3.3V range
    
    // Primeira leitura
    _voltage = _readVoltage();
    _percentage = _voltageToPercentage(_voltage);
    
    DEBUG_PRINTF("[PowerManager] Tensão inicial: %.2fV (%.1f%%)\n", 
                 _voltage, _percentage);
    
    return true;
}

void PowerManager::update() {
    uint32_t currentTime = millis();
    
    // Atualizar a cada segundo
    if (currentTime - _lastReadTime >= 1000) {
        _lastReadTime = currentTime;
        
        // Ler nova tensão
        float newVoltage = _readVoltage();
        
        // Atualizar estatísticas
        _sampleCount++;
        _avgVoltage = (_avgVoltage * (_sampleCount - 1) + newVoltage) / _sampleCount;
        
        if (newVoltage < _minVoltage) _minVoltage = newVoltage;
        if (newVoltage > _maxVoltage) _maxVoltage = newVoltage;
        
        // Atualizar valores
        _voltage = newVoltage;
        _percentage = _voltageToPercentage(_voltage);
        
        // Verificar níveis críticos
        if (isCritical()) {
            DEBUG_PRINTLN("[PowerManager] ALERTA: Bateria em nível CRÍTICO!");
        } else if (isLow()) {
            DEBUG_PRINTLN("[PowerManager] AVISO: Bateria em nível BAIXO");
        }
    }
}

float PowerManager::getVoltage() {
    return _voltage;
}

float PowerManager::getPercentage() {
    return _percentage;
}

float PowerManager::getCurrent() {
    // Implementação futura com sensor de corrente
    return 0.0;
}

float PowerManager::getPower() {
    // P = V * I
    return _voltage * getCurrent();
}

uint16_t PowerManager::getTimeRemaining() {
    // Estimativa baseada em consumo médio
    // Implementação simplificada - melhorar com dados reais
    float batteryCapacity = 2000.0;  // mAh (exemplo)
    float avgCurrent = 100.0;         // mA (estimado)
    
    if (avgCurrent <= 0) return 0;
    
    float remainingCapacity = (batteryCapacity * _percentage) / 100.0;
    float timeHours = remainingCapacity / avgCurrent;
    
    return (uint16_t)(timeHours * 60);  // Retorna em minutos
}

bool PowerManager::isCritical() {
    return _voltage < BATTERY_CRITICAL;
}

bool PowerManager::isLow() {
    return _voltage < BATTERY_LOW;
}

void PowerManager::enablePowerSave() {
    _powerSaveEnabled = true;
    
    // Reduzir frequência da CPU
    setCpuFrequencyMhz(80);  // 240 MHz -> 80 MHz
    
    DEBUG_PRINTLN("[PowerManager] Modo economia ativado (80 MHz)");
}

void PowerManager::disablePowerSave() {
    _powerSaveEnabled = false;
    
    // Restaurar frequência da CPU
    setCpuFrequencyMhz(240);
    
    DEBUG_PRINTLN("[PowerManager] Modo economia desativado (240 MHz)");
}

void PowerManager::deepSleep(uint64_t durationSeconds) {
    DEBUG_PRINTF("[PowerManager] Entrando em deep sleep por %llu segundos\n", 
                 durationSeconds);
    
    // Configurar timer de wake-up
    esp_sleep_enable_timer_wakeup(durationSeconds * 1000000ULL);
    
    // Entrar em deep sleep
    esp_deep_sleep_start();
}

void PowerManager::getStatistics(float& avgVoltage, float& minVoltage, float& maxVoltage) {
    avgVoltage = _avgVoltage;
    minVoltage = _minVoltage;
    maxVoltage = _maxVoltage;
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

float PowerManager::_readVoltage() {
    float sum = 0.0;
    
    // Múltiplas leituras para filtrar ruído
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        int rawValue = analogRead(BATTERY_PIN);
        sum += rawValue;
        delayMicroseconds(100);
    }
    
    float avgValue = sum / BATTERY_SAMPLES;
    
    // Converter para tensão
    float voltage = (avgValue / 4095.0) * BATTERY_VREF * BATTERY_DIVIDER;
    
    return voltage;
}

float PowerManager::_voltageToPercentage(float voltage) {
    // Curva de descarga Li-Ion simplificada
    // Para precisão, usar curva real da bateria
    
    if (voltage >= BATTERY_MAX_VOLTAGE) return 100.0;
    if (voltage <= BATTERY_MIN_VOLTAGE) return 0.0;
    
    // Interpolação linear (simplificado)
    float range = BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE;
    float percentage = ((voltage - BATTERY_MIN_VOLTAGE) / range) * 100.0;
    
    return constrain(percentage, 0.0, 100.0);
}
