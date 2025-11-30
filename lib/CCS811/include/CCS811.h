/**
 * @file CCS811.h
 * @brief Driver nativo do sensor CCS811 para Arduino/ESP32
 * @version 1.0.0
 * @date 2025-11-30
 * 
 * SENSOR: CCS811 - Air Quality Sensor (eCO2 e TVOC)
 * - eCO2: 400 a 8192 ppm (equivalente de CO2)
 * - TVOC: 0 a 1187 ppb (compostos orgânicos voláteis totais)
 * - Interface: I2C (endereços 0x5A ou 0x5B)
 * - Warm-up: 20 minutos para máxima precisão (48h para baseline estável)
 * 
 * CARACTERÍSTICAS:
 * - Compensação ambiental (temperatura e umidade)
 * - Modo de medição configurável (idle, 1s, 10s, 60s, 250ms)
 * - Baseline automática (com save/restore opcional)
 * - Detecção de erros com códigos específicos
 */

#ifndef CCS811_H
#define CCS811_H

#include <Arduino.h>
#include <Wire.h>
#define DEBUG_CCS811  // Ativa debug detalhado

class CCS811 {
public:
    // Endereços I2C possíveis (pino ADDR LOW/HIGH)
    static constexpr uint8_t I2C_ADDR_LOW = 0x5A;   // ADDR -> GND
    static constexpr uint8_t I2C_ADDR_HIGH = 0x5B;  // ADDR -> VDD
    
    // Modos de operação (taxa de amostragem)
    enum class DriveMode : uint8_t {
        IDLE = 0x00,      // Medições desabilitadas
        MODE_1SEC = 0x01, // 1 medição/segundo
        MODE_10SEC = 0x02, // 1 medição/10 segundos
        MODE_60SEC = 0x03, // 1 medição/60 segundos
        MODE_250MS = 0x04  // 1 medição/250 ms (raw data)
    };
    
    // Códigos de erro do sensor
    enum class ErrorCode : uint8_t {
        WRITE_REG_INVALID = 0x01,
        READ_REG_INVALID = 0x02,
        MEASMODE_INVALID = 0x04,
        MAX_RESISTANCE = 0x08,
        HEATER_FAULT = 0x10,
        HEATER_SUPPLY = 0x20
    };
    
    // Construtor - aceita referência customizada ao Wire ou usa padrão
    explicit CCS811(TwoWire& wirePort = Wire);
    
    // Inicialização
    bool begin(uint8_t i2cAddress = I2C_ADDR_LOW);
    void reset(); // Software reset
    
    // Configuração
    bool setDriveMode(DriveMode mode);
    bool setEnvironmentalData(float humidity, float temperature); // %RH, °C
    
    // Leitura de dados
    bool available();      // Verifica se há dados prontos
    bool readData();       // Lê eCO2 e TVOC (retorna true se sucesso)
    uint16_t geteCO2() const { return _eco2; }   // ppm
    uint16_t getTVOC() const { return _tvoc; }   // ppb
    
    // Baseline (calibração de longo prazo)
    bool getBaseline(uint16_t& baseline);
    bool setBaseline(uint16_t baseline);
    
    // Status e diagnóstico
    bool isInitialized() const { return _initialized; }
    uint8_t getHardwareID();
    uint8_t getHardwareVersion();
    uint16_t getBootloaderVersion();
    uint16_t getApplicationVersion();
    uint8_t getErrorCode();
    bool checkError();
    
    // Raw data (somente modo 250ms)
    bool readRawData(uint16_t& current, uint16_t& voltage);
    
private:
    // Registradores do CCS811 (datasheet seção 4)
    static constexpr uint8_t REG_STATUS = 0x00;
    static constexpr uint8_t REG_MEAS_MODE = 0x01;
    static constexpr uint8_t REG_ALG_RESULT_DATA = 0x02;
    static constexpr uint8_t REG_RAW_DATA = 0x03;
    static constexpr uint8_t REG_ENV_DATA = 0x05;
    static constexpr uint8_t REG_NTC = 0x06;
    static constexpr uint8_t REG_THRESHOLDS = 0x10;
    static constexpr uint8_t REG_BASELINE = 0x11;
    static constexpr uint8_t REG_HW_ID = 0x20;
    static constexpr uint8_t REG_HW_VERSION = 0x21;
    static constexpr uint8_t REG_FW_BOOT_VERSION = 0x23;
    static constexpr uint8_t REG_FW_APP_VERSION = 0x24;
    static constexpr uint8_t REG_ERROR_ID = 0xE0;
    static constexpr uint8_t REG_APP_ERASE = 0xF1;
    static constexpr uint8_t REG_APP_DATA = 0xF2;
    static constexpr uint8_t REG_APP_VERIFY = 0xF3;
    static constexpr uint8_t REG_APP_START = 0xF4;
    static constexpr uint8_t REG_SW_RESET = 0xFF;
    
    // Constantes
    static constexpr uint8_t HW_ID_CODE = 0x81;
    static constexpr uint32_t SW_RESET_SEQUENCE[4] = {0x11, 0xE5, 0x72, 0x8A};
    
    // Bits de STATUS
    static constexpr uint8_t STATUS_ERROR = 0x01;
    static constexpr uint8_t STATUS_DATA_READY = 0x08;
    static constexpr uint8_t STATUS_APP_VALID = 0x10;
    static constexpr uint8_t STATUS_FW_MODE = 0x80;
    
    // Hardware
    TwoWire* _wire;
    uint8_t _i2cAddress;
    bool _initialized;
    
    // Dados atuais
    uint16_t _eco2;  // ppm
    uint16_t _tvoc;  // ppb
    
    // Métodos internos - I2C
    bool _readRegister(uint8_t reg, uint8_t& value);
    bool _readRegisters(uint8_t reg, uint8_t* buffer, size_t length);
    bool _writeRegister(uint8_t reg, uint8_t value);
    bool _writeRegisters(uint8_t reg, const uint8_t* buffer, size_t length);
    bool _writeCommand(uint8_t command);
    
    // Métodos internos - Inicialização
    bool _checkHardwareID();
    bool _verifyAppValid();
    bool _startApp();
    bool _waitForAppMode(uint32_t timeoutMs = 1000);
    
    // Métodos internos - Conversão
    void _encodeEnvironmentalData(float humidity, float temperature, uint8_t* buffer);
};

#endif // CCS811_H
