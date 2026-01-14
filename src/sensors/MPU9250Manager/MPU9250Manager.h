/**
 * @file MPU9250Manager.h
 * @brief Gerenciador do sensor IMU MPU9250 (9-DOF) com calibração avançada
 * 
 * @details Driver completo para o MPU9250 com suporte a:
 *          - Acelerômetro 3 eixos (±2/4/8/16g)
 *          - Giroscópio 3 eixos (±250/500/1000/2000°/s)
 *          - Magnetômetro AK8963 (±4800µT)
 *          - Calibração Hard Iron (offset de bias)
 *          - Calibração Soft Iron (correção de distorção elipsoidal)
 *          - Filtro de média móvel para acelerômetro
 *          - Persistência de calibração em NVS
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Calibração do Magnetômetro
 * O magnetômetro requer calibração para compensar:
 * 
 * ### Hard Iron
 * Campos magnéticos fixos causados por materiais ferromagnéticos
 * próximos ao sensor. Corrigido subtraindo offset (bias).
 * 
 * ### Soft Iron  
 * Distorção do campo causada por materiais que alteram a forma
 * do campo (elipsóide → esfera). Corrigido com matriz 3x3.
 * 
 * ## Uso
 * @code{.cpp}
 * MPU9250Manager imu(0x68);
 * imu.begin();
 * imu.calibrateMagnetometer();  // 20 segundos, girar em figura 8
 * 
 * // No loop:
 * imu.update();
 * float heading = atan2(imu.getMagY(), imu.getMagX()) * RAD_TO_DEG;
 * @endcode
 * 
 * @note Endereço I2C padrão: 0x68 (AD0=LOW) ou 0x69 (AD0=HIGH)
 * @warning Calibração aloca ~6KB no heap temporariamente
 */

#ifndef MPU9250MANAGER_H
#define MPU9250MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include "MPU9250.h"
#include "config.h"

/**
 * @class MPU9250Manager
 * @brief Driver para IMU MPU9250 com calibração Hard + Soft Iron
 */
class MPU9250Manager {
public:
    /**
     * @brief Construtor
     * @param addr Endereço I2C (0x68 ou 0x69)
     */
    explicit MPU9250Manager(uint8_t addr = 0x69);

    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o sensor e carrega calibração da NVS
     * @return true se MPU9250 detectado e configurado
     * @return false se falha na comunicação I2C
     */
    bool begin();
    
    /**
     * @brief Atualiza leituras dos sensores (chamar a cada 20ms+)
     * @note Aplica filtro no acelerômetro e correção no magnetômetro
     */
    void update();
    
    /**
     * @brief Reinicializa o sensor após falha
     */
    void reset();

    //=========================================================================
    // GETTERS - DADOS CALIBRADOS
    //=========================================================================
    
    /** @name Acelerômetro (g) */
    ///@{
    float getAccelX() const { return _accelX; }  ///< Aceleração eixo X
    float getAccelY() const { return _accelY; }  ///< Aceleração eixo Y
    float getAccelZ() const { return _accelZ; }  ///< Aceleração eixo Z
    ///@}
    
    /** @name Giroscópio (°/s) */
    ///@{
    float getGyroX() const { return _gyroX; }    ///< Velocidade angular X
    float getGyroY() const { return _gyroY; }    ///< Velocidade angular Y
    float getGyroZ() const { return _gyroZ; }    ///< Velocidade angular Z
    ///@}
    
    /** @name Magnetômetro (µT) - Calibrado */
    ///@{
    float getMagX() const { return _magX; }      ///< Campo magnético X
    float getMagY() const { return _magY; }      ///< Campo magnético Y
    float getMagZ() const { return _magZ; }      ///< Campo magnético Z
    ///@}

    /**
     * @brief Obtém todos os dados de uma vez
     * @param[out] gx,gy,gz Giroscópio (°/s)
     * @param[out] ax,ay,az Acelerômetro (g)
     * @param[out] mx,my,mz Magnetômetro (µT)
     */
    void getRawData(float& gx, float& gy, float& gz, 
                    float& ax, float& ay, float& az,
                    float& mx, float& my, float& mz) const;

    //=========================================================================
    // STATUS
    //=========================================================================
    
    bool isOnline() const { return _online; }           ///< IMU respondendo?
    bool isMagOnline() const { return _magOnline; }     ///< Magnetômetro OK?
    bool isCalibrated() const { return _calibrated; }   ///< Calibração carregada?
    uint8_t getFailCount() const { return _failCount; } ///< Contador de falhas

    //=========================================================================
    // CALIBRAÇÃO DO MAGNETÔMETRO
    //=========================================================================
    
    /**
     * @brief Executa calibração completa (Hard + Soft Iron)
     * 
     * @return true se calibração bem sucedida (>200 amostras)
     * @return false se falhou ou memória insuficiente
     * 
     * @details Processo de 20 segundos:
     *          1. Coleta ~500 amostras enquanto usuário gira sensor
     *          2. Calcula Hard Iron (centro da elipse)
     *          3. Calcula Soft Iron (matriz de correção)
     *          4. Salva na NVS para persistência
     * 
     * @note Aloca ~6KB temporariamente no heap
     * @warning Girar sensor lentamente em figura 8 durante calibração
     */
    bool calibrateMagnetometer();
    
    /**
     * @brief Obtém offsets de Hard Iron
     * @param[out] x,y,z Offsets em µT
     */
    void getMagOffsets(float& x, float& y, float& z) const;
    
    /**
     * @brief Apaga calibração salva na NVS
     */
    void clearOffsetsFromMemory();

private:
    //=========================================================================
    // HARDWARE
    //=========================================================================
    MPU9250 _mpu;           ///< Instância do driver MPU9250
    uint8_t _addr;          ///< Endereço I2C
    
    //=========================================================================
    // ESTADO
    //=========================================================================
    bool _online;           ///< IMU principal online
    bool _magOnline;        ///< Magnetômetro AK8963 online
    bool _calibrated;       ///< Calibração carregada da NVS
    uint8_t _failCount;     ///< Contador de falhas consecutivas
    unsigned long _lastRead;///< Timestamp última leitura

    //=========================================================================
    // DADOS PROCESSADOS
    //=========================================================================
    float _accelX, _accelY, _accelZ;  ///< Acelerômetro filtrado (g)
    float _gyroX, _gyroY, _gyroZ;     ///< Giroscópio (°/s)
    float _magX, _magY, _magZ;        ///< Magnetômetro calibrado (µT)

    //=========================================================================
    // CALIBRAÇÃO DO MAGNETÔMETRO
    //=========================================================================
    float _magOffX, _magOffY, _magOffZ;  ///< Hard Iron offsets (bias)
    float _softIronMatrix[3][3];          ///< Soft Iron correction matrix

    //=========================================================================
    // FILTRO DE MÉDIA MÓVEL (ACELERÔMETRO)
    //=========================================================================
    static constexpr uint8_t FILTER_SIZE = 5;  ///< Tamanho do buffer
    float _bufAX[FILTER_SIZE];  ///< Buffer eixo X
    float _bufAY[FILTER_SIZE];  ///< Buffer eixo Y
    float _bufAZ[FILTER_SIZE];  ///< Buffer eixo Z
    uint8_t _filterIdx;         ///< Índice circular
    
    //=========================================================================
    // PERSISTÊNCIA (NVS)
    //=========================================================================
    Preferences _prefs;                              ///< Handle NVS
    static constexpr const char* PREFS_NAME = "mpu_mag";  ///< Namespace NVS
    static constexpr uint32_t MAGIC_KEY = 0xCAFEBABE;     ///< Validação

    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    bool _loadOffsets();    ///< Carrega calibração da NVS
    bool _saveOffsets();    ///< Salva calibração na NVS
    float _applyFilter(float val, float* buf);  ///< Aplica média móvel
    
    void _applySoftIronCorrection(float& mx, float& my, float& mz);  ///< Corrige distorção
    void _calculateSoftIronMatrix(float samples[][3], int numSamples);  ///< Calcula matriz
};

#endif