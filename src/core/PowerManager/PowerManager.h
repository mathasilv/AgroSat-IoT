/**
 * @file PowerManager.h
 * @brief Gerenciador de Energia com Curva de Descarga Li-ion Real
 * @version 2.0.0 (MODERADO 4.4 - Curva Não-Linear Implementada)
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "config.h"

class PowerManager {
public:
    PowerManager();

    bool begin();
    void update();

    float getVoltage() const { return _voltage; }
    float getPercentage() const { return _percentage; }

    // Status com Histerese
    bool isCritical() const { return _isCritical; }
    bool isLow() const { return _isLow; }

    // Controle de Energia
    void enablePowerSave();
    void disablePowerSave();
    
    // NOVO - 5.2: Ajuste dinâmico de CPU baseado em carga
    void adjustCpuFrequency();

private:
    float _voltage;
    float _percentage;
    
    bool _isCritical;
    bool _isLow;
    bool _powerSaveEnabled;

    float _avgVoltage;
    uint32_t _lastUpdate;
    
    static constexpr uint32_t UPDATE_INTERVAL = 1000;
    static constexpr float HYSTERESIS = 0.1f;

    float _readVoltage();
    
    // CORRIGIDO 4.4: Curva de descarga Li-ion 18650 real
    float _calculatePercentage(float voltage);
    
    void _updateStatus(float voltage);
};

#endif