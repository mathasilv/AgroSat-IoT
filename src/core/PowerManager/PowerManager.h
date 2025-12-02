/**
 * @file PowerManager.h
 * @brief Gerenciador de Energia com Histerese e Filtro
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

private:
    float _voltage;
    float _percentage;
    
    // Flags de Estado (Histerese)
    bool _isCritical;
    bool _isLow;
    bool _powerSaveEnabled;

    // Filtro
    float _avgVoltage;
    uint32_t _lastUpdate;
    
    // Configurações Internas
    static constexpr uint32_t UPDATE_INTERVAL = 1000;
    
    // Histerese (Margem de recuperação)
    static constexpr float HYSTERESIS = 0.1f; // 0.1V

    float _readVoltage();
    float _calculatePercentage(float voltage);
    void _updateStatus(float voltage);
};

#endif