/**
 * @file BMP280Manager.h
 * @brief Gerenciador do sensor barométrico BMP280
 * 
 * @details Driver completo para o sensor BMP280 da Bosch com suporte a:
 *          - Medição de pressão atmosférica (300-1100 hPa)
 *          - Medição de temperatura (-40°C a +85°C)
 *          - Cálculo de altitude barométrica
 *          - Detecção de sensor travado (frozen readings)
 *          - Validação de taxa de mudança (rate-of-change)
 *          - Filtro de outliers com histórico
 *          - Auto-reinicialização em caso de falha
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Especificações do Sensor
 * | Parâmetro       | Faixa           | Resolução    |
 * |-----------------|-----------------|-------------|
 * | Pressão         | 300-1100 hPa    | 0.01 hPa    |
 * | Temperatura     | -40°C a +85°C   | 0.01°C      |
 * | Altitude        | -500m a 9000m   | ~1m         |
 * 
 * ## Detecção de Anomalias
 * O driver implementa múltiplas camadas de validação:
 * 1. **Range check**: Valores dentro dos limites físicos
 * 2. **Rate-of-change**: Variação máxima por segundo
 * 3. **Frozen detection**: Leituras idênticas consecutivas
 * 4. **Outlier filter**: Mediana do histórico
 * 
 * @note Endereço I2C: 0x76 (SDO=GND) ou 0x77 (SDO=VCC)
 * @warning Requer período de warmup de 2 segundos após inicialização
 */

#ifndef BMP280MANAGER_H
#define BMP280MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "BMP280.h"
#include "config.h"

/**
 * @class BMP280Manager
 * @brief Driver para sensor barométrico BMP280 com validação avançada
 */
class BMP280Manager {
public:
    /**
     * @brief Construtor padrão
     */
    BMP280Manager();
    
    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o sensor BMP280
     * @return true se sensor detectado e configurado
     * @return false se falha na comunicação I2C
     */
    bool begin();
    
    /**
     * @brief Atualiza leituras do sensor
     * @note Inclui validação e detecção de anomalias
     */
    void update();
    
    /**
     * @brief Reinicia o sensor após falha
     */
    void reset();
    
    /**
     * @brief Força reinicialização completa
     * @note Respeita cooldown de 10 segundos entre tentativas
     */
    void forceReinit();
    
    //=========================================================================
    // GETTERS DE DADOS
    //=========================================================================
    
    /** @brief Retorna temperatura em °C */
    float getTemperature() const { return _temperature; }
    
    /** @brief Retorna pressão atmosférica em hPa */
    float getPressure() const { return _pressure; }
    
    /** @brief Retorna altitude barométrica em metros */
    float getAltitude() const { return _altitude; }
    
    //=========================================================================
    // STATUS
    //=========================================================================
    
    /** @brief Sensor respondendo? */
    bool isOnline() const { return _online; }
    
    /** @brief Última leitura de temperatura válida? */
    bool isTempValid() const { return _tempValid; }
    
    /** @brief Contador de falhas consecutivas */
    uint8_t getFailCount() const { return _failCount; }
    
private:
    //=========================================================================
    // HARDWARE
    //=========================================================================
    BMP280 _bmp280;              ///< Instância do driver BMP280
    
    //=========================================================================
    // DADOS PROCESSADOS
    //=========================================================================
    float _temperature;          ///< Temperatura atual (°C)
    float _pressure;             ///< Pressão atual (hPa)
    float _altitude;             ///< Altitude barométrica (m)
    
    //=========================================================================
    // ESTADO
    //=========================================================================
    bool _online;                ///< Sensor respondendo
    bool _tempValid;             ///< Última leitura válida
    uint8_t _failCount;          ///< Contador de falhas
    unsigned long _lastReinitTime; ///< Timestamp última reinicialização
    
    //=========================================================================
    // HISTÓRICO PARA FILTRO DE OUTLIERS
    //=========================================================================
    static constexpr uint8_t HISTORY_SIZE = 10;  ///< Tamanho do buffer circular
    float _pressureHistory[HISTORY_SIZE];   ///< Histórico de pressão
    float _altitudeHistory[HISTORY_SIZE];   ///< Histórico de altitude
    float _tempHistory[HISTORY_SIZE];       ///< Histórico de temperatura
    uint8_t _historyIndex;                  ///< Índice circular
    bool _historyFull;                      ///< Buffer cheio?
    unsigned long _lastUpdateTime;          ///< Timestamp última leitura
    unsigned long _warmupStartTime;         ///< Início do warmup

    //=========================================================================
    // DETECÇÃO DE SENSOR TRAVADO
    //=========================================================================
    float _lastPressureRead;                ///< Última pressão lida
    uint16_t _identicalReadings;            ///< Contador de leituras idênticas
    
    //=========================================================================
    // CONSTANTES DE CONFIGURAÇÃO
    //=========================================================================
    static constexpr unsigned long WARMUP_DURATION = 2000;      ///< Warmup: 2s
    static constexpr unsigned long REINIT_COOLDOWN = 10000;     ///< Cooldown: 10s
    static constexpr uint16_t MAX_IDENTICAL_READINGS = 200;     ///< Limite frozen
    
    //=========================================================================
    // LIMITES DO DATASHEET
    //=========================================================================
    static constexpr float TEMP_MIN = -60.0f;       ///< Temperatura mínima (°C)
    static constexpr float TEMP_MAX = 85.0f;        ///< Temperatura máxima (°C)
    static constexpr float PRESSURE_MIN = 5.0f;     ///< Pressão mínima (hPa)
    static constexpr float PRESSURE_MAX = 1100.0f;  ///< Pressão máxima (hPa)
    
    //=========================================================================
    // TAXAS MÁXIMAS DE MUDANÇA (FÍSICA)
    //=========================================================================
    static constexpr float MAX_PRESSURE_RATE = 5.0f;   ///< Max Δpressão (hPa/s)
    static constexpr float MAX_ALTITUDE_RATE = 50.0f;  ///< Max Δaltitude (m/s)
    static constexpr float MAX_TEMP_RATE = 1.0f;       ///< Max Δtemp (°C/s)
    
    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    
    /** @brief Lê valores raw do sensor */
    bool _readRaw(float& temp, float& press, float& alt);
    
    /** @brief Valida leitura contra limites do datasheet */
    bool _validateReading(float temp, float press, float alt);
    
    /** @brief Verifica taxa de mudança física plausível */
    bool _checkRateOfChange(float temp, float press, float alt, float deltaTime);
    
    /** @brief Detecta sensor travado (leituras idênticas) */
    bool _isFrozen(float currentPressure);
    
    /** @brief Verifica se valor é outlier comparado ao histórico */
    bool _isOutlier(float value, float* history, uint8_t count) const;
    
    /** @brief Calcula mediana de array */
    float _getMedian(float* values, uint8_t count) const;
    
    /** @brief Atualiza buffer circular de histórico */
    void _updateHistory(float temp, float press, float alt);
    
    /** @brief Inicializa valores do histórico */
    void _initHistoryValues();
    
    /** @brief Verifica se pode reinicializar (cooldown) */
    bool _canReinit() const;
};

#endif // BMP280MANAGER_H