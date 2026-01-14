/**
 * @file SI7021Manager.h
 * @brief Gerenciador do sensor de umidade e temperatura SI7021
 * 
 * @details Driver para o sensor SI7021 da Silicon Labs com suporte a:
 *          - Medição de umidade relativa (0-100% RH)
 *          - Medição de temperatura (-40°C a +125°C)
 *          - Validação de leituras contra limites físicos
 *          - Auto-recuperação em caso de falha
 *          - Rate limiting para preservar vida útil do sensor
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Especificações do Sensor
 * | Parâmetro       | Faixa           | Precisão     |
 * |-----------------|-----------------|-------------|
 * | Umidade         | 0-100% RH       | ±3% RH      |
 * | Temperatura     | -40°C a +125°C  | ±0.4°C      |
 * | Tempo de medição| ~12ms (RH)      | ~7ms (Temp) |
 * 
 * ## Características
 * - Interface I2C (endereço fixo 0x40)
 * - Baixo consumo de energia
 * - Aquecedor interno para desumidificação
 * 
 * @note Endereço I2C fixo: 0x40
 * @note Intervalo mínimo entre leituras: 2 segundos
 */

#ifndef SI7021MANAGER_H
#define SI7021MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "SI7021.h" // Inclui o driver
#include "config.h"

/**
 * @class SI7021Manager
 * @brief Driver para sensor de umidade/temperatura SI7021
 */
class SI7021Manager {
public:
    /**
     * @brief Construtor padrão
     */
    SI7021Manager();

    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o sensor SI7021
     * @return true se sensor detectado no endereço 0x40
     * @return false se falha na comunicação I2C
     */
    bool begin();
    
    /**
     * @brief Atualiza leituras do sensor
     * @note Respeita intervalo mínimo de 2 segundos entre leituras
     */
    void update();
    
    /**
     * @brief Reinicia o sensor após falha
     */
    void reset();

    //=========================================================================
    // GETTERS DE DADOS
    //=========================================================================
    
    /** @brief Retorna temperatura em °C */
    float getTemperature() const { return _lastTemp; }
    
    /** @brief Retorna umidade relativa em % */
    float getHumidity() const { return _lastHum; }

    //=========================================================================
    // STATUS
    //=========================================================================
    
    /** @brief Sensor respondendo? */
    bool isOnline() const { return _online; }
    
    /** @brief Última leitura de temperatura válida? */
    bool isTempValid() const { return _online && !isnan(_lastTemp); }

private:
    //=========================================================================
    // HARDWARE
    //=========================================================================
    SI7021 _si7021;              ///< Instância do driver SI7021

    //=========================================================================
    // ESTADO
    //=========================================================================
    bool _online;                ///< Sensor respondendo
    float _lastTemp;             ///< Última temperatura válida (°C)
    float _lastHum;              ///< Última umidade válida (%)
    uint8_t _failCount;          ///< Contador de falhas consecutivas
    unsigned long _lastRead;     ///< Timestamp última leitura

    //=========================================================================
    // CONSTANTES DE VALIDAÇÃO
    //=========================================================================
    static constexpr float TEMP_MIN = -90.0f;   ///< Temperatura mínima (°C)
    static constexpr float TEMP_MAX = 85.0f;    ///< Temperatura máxima (°C)
    static constexpr float HUM_MIN = 0.0f;      ///< Umidade mínima (%)
    static constexpr float HUM_MAX = 100.0f;    ///< Umidade máxima (%)
    
    static constexpr unsigned long READ_INTERVAL_MS = 2000;  ///< Intervalo entre leituras
};

#endif // SI7021MANAGER_H