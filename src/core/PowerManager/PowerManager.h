/**
 * @file PowerManager.h
 * @brief Gerenciador de energia com curva de descarga Li-ion real
 * 
 * @details Sistema de monitoramento de bateria com suporte a:
 *          - Leitura de tensão via ADC com média móvel
 *          - Curva de descarga não-linear para Li-ion 18650
 *          - Detecção de níveis críticos com histerese
 *          - Ajuste dinâmico de frequência da CPU
 *          - Modo de economia de energia
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Curva de Descarga Li-ion 18650
 * | Tensão (V) | Capacidade (%) | Estado       |
 * |------------|----------------|-------------|
 * | 4.20       | 100%           | Cheio       |
 * | 4.00       | 80%            | Bom         |
 * | 3.80       | 45%            | Médio       |
 * | 3.70       | 25%            | Baixo       |
 * | 3.50       | 5%             | Crítico     |
 * | 3.30       | 0%             | Desligando  |
 * 
 * ## Ajuste Dinâmico de CPU
 * | Bateria    | Frequência | Consumo  |
 * |------------|------------|----------|
 * | > 60%      | 240 MHz    | ~80 mA   |
 * | 30-60%     | 160 MHz    | ~50 mA   |
 * | 15-30%     | 80 MHz     | ~30 mA   |
 * | < 15%      | 80 MHz     | ~30 mA   |
 * 
 * @note Requer divisor de tensão no pino BATTERY_PIN
 * @warning Calibrar BATTERY_VREF e BATTERY_DIVIDER para seu hardware
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include "config.h"

/**
 * @class PowerManager
 * @brief Gerenciador de energia com suporte a Li-ion e economia de energia
 */
class PowerManager {
public:
    /**
     * @brief Construtor padrão
     */
    PowerManager();

    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o ADC para leitura de bateria
     * @return true sempre
     */
    bool begin();
    
    /**
     * @brief Atualiza leitura de tensão e status
     * @note Usa média móvel para estabilidade
     */
    void update();

    //=========================================================================
    // GETTERS DE DADOS
    //=========================================================================
    
    /** @brief Retorna tensão da bateria em Volts */
    float getVoltage() const { return _voltage; }
    
    /** @brief Retorna percentual de carga (0-100%) */
    float getPercentage() const { return _percentage; }

    //=========================================================================
    // STATUS (COM HISTERESE)
    //=========================================================================
    
    /** @brief Bateria em nível crítico? (< 3.3V) */
    bool isCritical() const { return _isCritical; }
    
    /** @brief Bateria em nível baixo? (< 3.7V) */
    bool isLow() const { return _isLow; }

    //=========================================================================
    // CONTROLE DE ENERGIA
    //=========================================================================
    
    /**
     * @brief Ativa modo de economia de energia
     * @details Reduz CPU para 80MHz
     */
    void enablePowerSave();
    
    /**
     * @brief Desativa modo de economia de energia
     * @details Restaura CPU para 240MHz
     */
    void disablePowerSave();
    
    /**
     * @brief Ajusta frequência da CPU baseado no nível de bateria
     * @details Chamado automaticamente no update()
     */
    void adjustCpuFrequency();

private:
    //=========================================================================
    // ESTADO
    //=========================================================================
    float _voltage;              ///< Tensão atual (V)
    float _percentage;           ///< Percentual de carga (%)
    
    bool _isCritical;            ///< Flag: nível crítico
    bool _isLow;                 ///< Flag: nível baixo
    bool _powerSaveEnabled;      ///< Modo economia ativo

    float _avgVoltage;           ///< Média móvel de tensão
    uint32_t _lastUpdate;        ///< Timestamp última atualização
    
    //=========================================================================
    // CONSTANTES
    //=========================================================================
    static constexpr uint32_t UPDATE_INTERVAL = 1000;  ///< Intervalo de atualização (ms)
    static constexpr float HYSTERESIS = 0.1f;          ///< Histerese para flags (V)

    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    
    /** @brief Lê tensão do ADC com média de amostras */
    float _readVoltage();
    
    /**
     * @brief Calcula percentual usando curva Li-ion real
     * @param voltage Tensão em Volts
     * @return Percentual de carga (0-100)
     */
    float _calculatePercentage(float voltage);
    
    /** @brief Atualiza flags de status com histerese */
    void _updateStatus(float voltage);
};

#endif