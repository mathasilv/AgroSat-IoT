/**
 * @file PowerManager.h
 * @brief Gerenciamento de energia e monitoramento de bateria
 * @version 1.0.0
 * 
 * Responsável por:
 * - Leitura do nível de bateria
 * - Cálculo de porcentagem e tempo restante
 * - Gerenciamento de modos de economia de energia
 * - Proteção contra descarga excessiva
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "config.h"

class PowerManager {
public:
    /**
     * @brief Construtor
     */
    PowerManager();
    
    /**
     * @brief Inicializa o gerenciador de energia
     * @return true se inicializado com sucesso
     */
    bool begin();
    
    /**
     * @brief Atualiza leituras de bateria (chamada no loop)
     */
    void update();
    
    /**
     * @brief Retorna tensão da bateria em Volts
     */
    float getVoltage();
    
    /**
     * @brief Retorna porcentagem de bateria (0-100%)
     */
    float getPercentage();
    
    /**
     * @brief Retorna corrente estimada (mA) - futuro
     */
    float getCurrent();
    
    /**
     * @brief Retorna potência consumida (mW) - futuro
     */
    float getPower();
    
    /**
     * @brief Retorna tempo estimado restante (minutos)
     */
    uint16_t getTimeRemaining();
    
    /**
     * @brief Verifica se bateria está em nível crítico
     */
    bool isCritical();
    
    /**
     * @brief Verifica se bateria está em nível baixo
     */
    bool isLow();
    
    /**
     * @brief Habilita modo economia de energia
     */
    void enablePowerSave();
    
    /**
     * @brief Desabilita modo economia de energia
     */
    void disablePowerSave();
    
    /**
     * @brief Entra em deep sleep (não usado durante voo)
     */
    void deepSleep(uint64_t durationSeconds);
    
    /**
     * @brief Retorna estatísticas de energia
     */
    void getStatistics(float& avgVoltage, float& minVoltage, float& maxVoltage);

private:
    float _voltage;
    float _percentage;
    float _avgVoltage;
    float _minVoltage;
    float _maxVoltage;
    uint32_t _lastReadTime;
    uint16_t _sampleCount;
    bool _powerSaveEnabled;
    
    /**
     * @brief Lê tensão da bateria com filtro
     */
    float _readVoltage();
    
    /**
     * @brief Converte tensão para porcentagem
     */
    float _voltageToPercentage(float voltage);
};

#endif // POWER_MANAGER_H
