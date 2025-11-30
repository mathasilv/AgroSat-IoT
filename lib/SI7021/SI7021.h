/**
 * @file SI7021.h
 * @brief Driver nativo SI7021 - Biblioteca de baixo nível
 * @version 1.0.1
 *
 * Observações:
 * - Assume que Wire.begin() já foi chamado no setup().
 * - Não usa alocação dinâmica.
 * - Não lança exceções.
 */

#ifndef SI7021_H
#define SI7021_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Endereço I2C padrão do Si7021
static constexpr uint8_t SI7021_I2C_ADDR = 0x40;

// Comandos principais (datasheet Si7021)
static constexpr uint8_t SI7021_CMD_MEASURE_RH_NOHOLD = 0xF5;  // Measure RH, no hold master
static constexpr uint8_t SI7021_CMD_MEASURE_T_NOHOLD  = 0xF3;  // Measure T, no hold master
static constexpr uint8_t SI7021_CMD_SOFT_RESET        = 0xFE;  // Soft reset
static constexpr uint8_t SI7021_CMD_READ_USER_REG     = 0xE7;  // Read user register

// Comandos para leitura de ID eletrônico (não usados diretamente, mas mantidos para futura expansão)
// Sequência correta (2 bytes): 0xFA 0x0F e 0xFC 0xC9 (ver datasheet)
static constexpr uint8_t SI7021_CMD_READ_ID1_MSB      = 0xFA;
static constexpr uint8_t SI7021_CMD_READ_ID1_LSB      = 0x0F;
static constexpr uint8_t SI7021_CMD_READ_ID2_MSB      = 0xFC;
static constexpr uint8_t SI7021_CMD_READ_ID2_LSB      = 0xC9;

class SI7021 {
public:
    /**
     * @brief Construtor
     * @param wire Instância de TwoWire (por padrão, Wire)
     */
    explicit SI7021(TwoWire& wire = Wire);

    /**
     * @brief Inicializa o driver do sensor
     * @param addr Endereço I2C (padrão 0x40)
     * @return true se o sensor foi detectado e passou no autoteste básico
     */
    bool begin(uint8_t addr = SI7021_I2C_ADDR);

    /**
     * @brief Lê a umidade relativa em %
     * @param humidity Saída (0.0 a 100.0 em caso de sucesso)
     * @return true em caso de sucesso; false em erro (humidity = -999.0)
     */
    bool readHumidity(float& humidity);

    /**
     * @brief Lê a temperatura em graus Celsius
     * @param temperature Saída em °C
     * @return true em caso de sucesso; false em erro (temperature = -999.0)
     */
    bool readTemperature(float& temperature);

    /**
     * @brief Indica se o sensor foi inicializado com sucesso
     */
    bool isOnline() const { return _online; }

    /**
     * @brief Retorna o "device ID" interno armazenado
     *
     * No momento, este campo é usado como snapshot de um registrador
     * interno simples (user register), ou 0 caso não tenha sido lido.
     */
    uint8_t getDeviceID() const { return _deviceID; }

    /**
     * @brief Executa soft reset no sensor
     */
    void reset();

private:
    TwoWire* _wire;
    uint8_t  _addr;
    bool     _online;
    uint8_t  _deviceID;

    bool _writeCommand(uint8_t cmd);
    bool _readBytes(uint8_t* buf, size_t len);
    bool _readRegister(uint8_t cmd, uint8_t* data, size_t len);
    bool _verifyPresence();
};

#endif // SI7021_H
