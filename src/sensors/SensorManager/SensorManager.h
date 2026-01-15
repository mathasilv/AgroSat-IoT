/**
 * @file SensorManager.h
 * @brief Gerenciador centralizado de sensores com orquestração I2C
 * 
 * @details Este módulo atua como orquestrador central para todos os sensores
 *          do sistema AgroSat-IoT. Coordena o acesso thread-safe ao barramento
 *          I2C através de mutex FreeRTOS e delega operações para managers
 *          específicos de cada sensor.
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Sensores Suportados
 * | Sensor   | Tipo                  | Interface | Endereço |
 * |----------|-----------------------|-----------|----------|
 * | MPU9250  | IMU 9-DOF             | I2C       | 0x68     |
 * | BMP280   | Pressão/Temperatura   | I2C       | 0x76     |
 * | SI7021   | Umidade/Temperatura   | I2C       | 0x40     |
 * | CCS811   | Qualidade do Ar       | I2C       | 0x5A     |
 * 
 * ## Ciclos de Atualização
 * - **updateFast()**: MPU9250 + BMP280 (50-100Hz)
 * - **updateSlow()**: SI7021 + CCS811 (1Hz)
 * - **updateHealth()**: Verificação de saúde (0.03Hz)
 * 
 * @see MPU9250Manager, BMP280Manager, SI7021Manager, CCS811Manager
 * 
 * @note Requer FreeRTOS para funcionamento correto dos mutexes
 * @warning Não chamar update() de múltiplas tasks sem proteção
 */

#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Includes dos Managers Específicos
#include "sensors/MPU9250Manager/MPU9250Manager.h"
#include "sensors/BMP280Manager/BMP280Manager.h"
#include "sensors/SI7021Manager/SI7021Manager.h"
#include "sensors/CCS811Manager/CCS811Manager.h"
#include "config.h"

/**
 * @class SensorManager
 * @brief Orquestrador central para todos os sensores do sistema
 */
class SensorManager {
public:
    /**
     * @brief Construtor padrão
     */
    SensorManager();

    /**
     * @brief Inicializa todos os sensores do sistema
     * @return true se pelo menos um sensor foi inicializado
     * @return false se nenhum sensor respondeu
     * @note Deve ser chamado após Wire.begin()
     */
    bool begin();

    //=========================================================================
    // CICLOS DE ATUALIZAÇÃO
    //=========================================================================
    
    /**
     * @brief Atualiza sensores de alta frequência (IMU + Barômetro)
     * @details Lê MPU9250 e BMP280 com proteção de mutex I2C.
     * @note Ideal para chamadas a 50-100Hz
     */
    void updateFast();
    
    /**
     * @brief Atualiza sensores de baixa frequência (Ambiente)
     * @details Lê SI7021 e CCS811 com proteção de mutex I2C.
     * @note Ideal para chamadas a 1Hz
     */
    void updateSlow();
    
    /**
     * @brief Executa verificação de saúde dos sensores
     * @details Verifica conectividade e reinicia sensores com falha.
     */
    void updateHealth();
    
    /**
     * @brief Wrapper que executa todos os ciclos de atualização
     */
    void update();

    //=========================================================================
    // CONTROLE E RESET
    //=========================================================================
    
    /**
     * @brief Reinicia sensores com falha
     */
    void reset();
    
    /**
     * @brief Força reinicialização de todos os sensores
     */
    void resetAll();

    //=========================================================================
    // CALIBRAÇÃO DO MAGNETÔMETRO
    //=========================================================================
    
    /**
     * @brief Executa calibração completa do magnetômetro (Hard + Soft Iron)
     * @return true se calibração bem sucedida (>200 amostras)
     * @return false se falhou ou poucas amostras
     * @note Processo leva ~20 segundos - girar sensor em figura 8
     */
    bool recalibrateMagnetometer();
    
    /**
     * @brief Apaga calibração salva na NVS
     */
    void clearMagnetometerCalibration();
    
    //=========================================================================
    // CONFIGURAÇÃO CCS811
    //=========================================================================
    
    /**
     * @brief Salva baseline do CCS811 na NVS
     * @return true se salvo com sucesso
     * @note Chamar após 20+ minutos de operação
     */
    bool saveCCS811Baseline();
    
    /**
     * @brief Restaura baseline do CCS811 da NVS
     * @return true se restaurado com sucesso
     */
    bool restoreCCS811Baseline();
    
    //=========================================================================
    // STATUS E DIAGNÓSTICO
    //=========================================================================
    

    
    /** @name Status Individuais dos Sensores */
    ///@{
    bool isMPU9250Online() const { return _mpu9250.isOnline(); }  ///< IMU online?
    bool isBMP280Online() const { return _bmp280.isOnline(); }    ///< Barômetro online?
    bool isSI7021Online() const { return _si7021.isOnline(); }    ///< Higrômetro online?
    bool isCCS811Online() const { return _ccs811.isOnline(); }    ///< Sensor CO2 online?
    ///@}

    void printDetailedStatus() const;
    
    //=========================================================================
    // GETTERS DE DADOS
    //=========================================================================
    
    /** @name Dados do MPU9250 (IMU 9-DOF) */
    ///@{
    float getAccelX() const { return _mpu9250.getAccelX(); }
    float getAccelY() const { return _mpu9250.getAccelY(); }
    float getAccelZ() const { return _mpu9250.getAccelZ(); }
    float getGyroX() const { return _mpu9250.getGyroX(); }
    float getGyroY() const { return _mpu9250.getGyroY(); }
    float getGyroZ() const { return _mpu9250.getGyroZ(); }
    float getMagX() const { return _mpu9250.getMagX(); }
    float getMagY() const { return _mpu9250.getMagY(); }
    float getMagZ() const { return _mpu9250.getMagZ(); }      ///< Campo mag Z (µT)
    ///@}
    
    /** @name Dados do BMP280 (Barômetro) */
    ///@{
    float getTemperature() const { return _bmp280.getTemperature(); }
    float getTemperatureBMP280() const { return _bmp280.getTemperature(); }
    float getPressure() const { return _bmp280.getPressure(); }
    float getAltitude() const { return _bmp280.getAltitude(); }  ///< Altitude (m)
    ///@}
    
    /** @name Dados do SI7021 (Higrômetro) */
    ///@{
    float getHumidity() const { return _si7021.getHumidity(); }
    float getTemperatureSI7021() const { return _si7021.getTemperature(); }  ///< Temp SI7021 (°C)
    ///@}
    
    /** @name Dados do CCS811 (Qualidade do Ar) */
    ///@{
    uint16_t geteCO2() const { return _ccs811.geteCO2(); }
    uint16_t getCO2() const { return _ccs811.geteCO2(); } // Alias
    uint16_t getTVOC() const { return _ccs811.getTVOC(); }  ///< TVOC (ppb)
    ///@}
    
private:
    //=========================================================================
    // MANAGERS DE SENSORES
    //=========================================================================
    MPU9250Manager _mpu9250;    ///< Manager do IMU 9-DOF
    BMP280Manager _bmp280;      ///< Manager do barômetro
    SI7021Manager _si7021;      ///< Manager do higrômetro
    CCS811Manager _ccs811;      ///< Manager do sensor de CO2
    
    //=========================================================================
    // VARIÁVEIS DE CONTROLE
    //=========================================================================
    uint8_t _sensorCount;               ///< Número de sensores configurados
    unsigned long _lastEnvCompensation; ///< Timestamp última compensação
    unsigned long _lastHealthCheck;     ///< Timestamp último health check
    uint8_t _consecutiveFailures;       ///< Contador de falhas consecutivas
    float _temperature;                 ///< Temperatura para redundância

    SemaphoreHandle_t _i2cMutex;        ///< Mutex para acesso I2C

    //=========================================================================
    // CONSTANTES
    //=========================================================================
    static constexpr unsigned long ENV_COMPENSATION_INTERVAL = 60000;  ///< 60s
    static constexpr unsigned long HEALTH_CHECK_INTERVAL = 30000;      ///< 30s
    static constexpr uint8_t MAX_CONSECUTIVE_FAILURES = 10;            ///< Limite de falhas

    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    void _autoApplyEnvironmentalCompensation();  ///< Aplica compensação automática
    void _updateTemperatureRedundancy();         ///< Atualiza temp redundante
    void _performHealthCheck();                  ///< Executa verificação de saúde
    
    bool _lockI2C();    ///< Adquire mutex I2C com timeout
    void _unlockI2C();  ///< Libera mutex I2C
};

#endif // SENSORMANAGER_H