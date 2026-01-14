/**
 * @file CCS811Manager.h
 * @brief Gerenciador do sensor de qualidade do ar CCS811
 * 
 * @details Driver para o sensor CCS811 da ams com suporte a:
 *          - Medição de eCO2 (CO2 equivalente: 400-8192 ppm)
 *          - Medição de TVOC (Compostos Orgânicos Voláteis: 0-1187 ppb)
 *          - Compensação ambiental (temperatura e umidade)
 *          - Persistência de baseline na NVS
 *          - Período de warmup para estabilização
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.2.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Especificações do Sensor
 * | Parâmetro | Faixa           | Resolução |
 * |-----------|-----------------|----------|
 * | eCO2      | 400-8192 ppm    | 1 ppm    |
 * | TVOC      | 0-1187 ppb      | 1 ppb    |
 * 
 * ## Modos de Operação
 * | Modo | Intervalo | Consumo  |
 * |------|-----------|----------|
 * | 1    | 1s        | 26 mA    |
 * | 2    | 10s       | 10 mA    |
 * | 3    | 60s       | 2 mA     |
 * 
 * ## Warmup e Baseline
 * - **Warmup**: 20 minutos para leituras confiáveis
 * - **Baseline**: Salvar após 20+ minutos de operação estável
 * - **Burn-in**: 48 horas para precisão máxima (primeiro uso)
 * 
 * @note Endereço I2C: 0x5A (ADDR=LOW) ou 0x5B (ADDR=HIGH)
 * @warning Dados não confiáveis durante período de warmup
 */

#ifndef CCS811MANAGER_H
#define CCS811MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "CCS811.h"
#include "config.h"

/**
 * @class CCS811Manager
 * @brief Driver para sensor de qualidade do ar CCS811
 */
class CCS811Manager {
public:
    /**
     * @brief Construtor padrão
     */
    CCS811Manager();

    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o sensor CCS811
     * @return true se sensor detectado e configurado
     * @return false se falha na comunicação I2C
     * @note Inicia período de warmup de 20 segundos
     */
    bool begin();
    
    /**
     * @brief Atualiza leituras do sensor
     * @note Respeita intervalo mínimo de 2 segundos
     */
    void update();
    
    /**
     * @brief Reinicia o sensor após falha
     */
    void reset();

    //=========================================================================
    // GETTERS DE DADOS
    //=========================================================================
    
    /** @brief Retorna eCO2 em ppm (400-8192) */
    uint16_t geteCO2() const { return _eco2; }
    
    /** @brief Retorna TVOC em ppb (0-1187) */
    uint16_t getTVOC() const { return _tvoc; }
    
    //=========================================================================
    // STATUS
    //=========================================================================
    
    /** @brief Sensor respondendo? */
    bool isOnline() const { return _online; }
    
    /**
     * @brief Verifica se dados são válidos
     * @return true se online E warmup completo
     */
    bool isDataValid() const;
    
    /**
     * @brief Alias para isDataValid()
     * @return true se dados são confiáveis
     */
    bool isDataReliable() const;
    
    /**
     * @brief Verifica se período de warmup terminou
     * @return true se passaram 20+ segundos desde begin()
     */
    bool isWarmupComplete() const;

    //=========================================================================
    // COMPENSAÇÃO E CALIBRAÇÃO
    //=========================================================================
    
    /**
     * @brief Aplica compensação ambiental para melhorar precisão
     * @param hum Umidade relativa em %
     * @param temp Temperatura em °C
     * @note Chamar periodicamente (a cada 60s) para melhor precisão
     */
    void setEnvironmentalData(float hum, float temp);
    
    /**
     * @brief Salva baseline atual na NVS
     * @return true se salvo com sucesso
     * @note Chamar após 20+ minutos de operação estável
     */
    bool saveBaseline();
    
    /**
     * @brief Restaura baseline da NVS
     * @return true se restaurado com sucesso
     * @note Chamar após begin() para acelerar estabilização
     */
    bool restoreBaseline();

private:
    //=========================================================================
    // HARDWARE
    //=========================================================================
    CCS811 _ccs811;              ///< Instância do driver CCS811

    //=========================================================================
    // ESTADO
    //=========================================================================
    bool _online;                ///< Sensor respondendo
    uint16_t _eco2;              ///< Último eCO2 válido (ppm)
    uint16_t _tvoc;              ///< Último TVOC válido (ppb)
    unsigned long _initTime;     ///< Timestamp de inicialização
    unsigned long _lastRead;     ///< Timestamp última leitura
    uint8_t _failCount;          ///< Contador de falhas consecutivas

    //=========================================================================
    // CONSTANTES
    //=========================================================================
    static constexpr unsigned long READ_INTERVAL = 2000;   ///< Intervalo entre leituras (ms)
    static constexpr unsigned long WARMUP_TIME = 20000;    ///< Tempo de warmup (20s)
};

#endif