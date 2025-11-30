#ifndef BMP280_H
#define BMP280_H

#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// BMP280 - Driver nativo para Arduino/ESP32
// Sensor de temperatura e pressão barométrica
// - Temperatura: -40 a +85°C (precisão ±1°C)
// - Pressão: 300 a 1100 hPa (precisão ±1 hPa)
// ============================================================================

class BMP280 {
public:
    // Endereços I2C possíveis (SDO conectado a GND ou VDD)
    static constexpr uint8_t I2C_ADDR_PRIMARY = 0x76;   // SDO -> GND
    static constexpr uint8_t I2C_ADDR_SECONDARY = 0x77; // SDO -> VDD
    
    // Modos de operação
    enum class Mode : uint8_t {
        SLEEP = 0x00,
        FORCED = 0x01,
        NORMAL = 0x03
    };
    
    // Oversampling de temperatura
    enum class TempOversampling : uint8_t {
        SKIP = 0x00,
        X1 = 0x01,
        X2 = 0x02,
        X4 = 0x03,
        X8 = 0x04,
        X16 = 0x05
    };
    
    // Oversampling de pressão
    enum class PressOversampling : uint8_t {
        SKIP = 0x00,
        X1 = 0x01,
        X2 = 0x02,
        X4 = 0x03,
        X8 = 0x04,
        X16 = 0x05
    };
    
    // Coeficiente do filtro IIR
    enum class Filter : uint8_t {
        OFF = 0x00,
        X2 = 0x01,
        X4 = 0x02,
        X8 = 0x03,
        X16 = 0x04
    };
    
    // Tempo de standby entre medições (modo NORMAL)
    enum class StandbyTime : uint8_t {
        MS_0_5 = 0x00,   // 0.5ms
        MS_62_5 = 0x01,  // 62.5ms
        MS_125 = 0x02,   // 125ms
        MS_250 = 0x03,   // 250ms
        MS_500 = 0x04,   // 500ms
        MS_1000 = 0x05,  // 1000ms
        MS_2000 = 0x06,  // 2000ms
        MS_4000 = 0x07   // 4000ms
    };

    // Construtor - aceita referência customizada ao Wire ou usa padrão
    explicit BMP280(TwoWire& wirePort = Wire);
    
    // Inicialização com endereço I2C especificado
    bool begin(uint8_t i2cAddress = I2C_ADDR_PRIMARY);
    
    // Configuração avançada (opcional - defaults já otimizados)
    bool configure(Mode mode = Mode::NORMAL,
                   TempOversampling tempOS = TempOversampling::X2,
                   PressOversampling pressOS = PressOversampling::X16,
                   Filter filter = Filter::X16,
                   StandbyTime standby = StandbyTime::MS_500);
    
    // Leituras principais
    float readTemperature();    // Retorna temperatura em °C
    float readPressure();       // Retorna pressão em Pa
    float readAltitude(float seaLevelPressure = 101325.0f); // Altitude em metros
    
    // Utilitários
    bool isInitialized() const { return _initialized; }
    void reset(); // Soft reset do sensor
    
private:
    // Registradores BMP280 (datasheet seção 4.3)
    static constexpr uint8_t REG_ID = 0xD0;
    static constexpr uint8_t REG_RESET = 0xE0;
    static constexpr uint8_t REG_STATUS = 0xF3;
    static constexpr uint8_t REG_CTRL_MEAS = 0xF4;
    static constexpr uint8_t REG_CONFIG = 0xF5;
    static constexpr uint8_t REG_PRESS_MSB = 0xF7;
    static constexpr uint8_t REG_TEMP_MSB = 0xFA;
    static constexpr uint8_t REG_CALIB_START = 0x88;
    
    static constexpr uint8_t CHIP_ID = 0x58;
    static constexpr uint8_t RESET_CMD = 0xB6;
    static constexpr uint8_t CALIB_DATA_SIZE = 24;
    
    TwoWire* _wire;
    uint8_t _i2cAddress;
    bool _initialized;
    
    // Coeficientes de calibração (datasheet seção 3.11.2)
    uint16_t _dig_T1;
    int16_t _dig_T2;
    int16_t _dig_T3;
    uint16_t _dig_P1;
    int16_t _dig_P2;
    int16_t _dig_P3;
    int16_t _dig_P4;
    int16_t _dig_P5;
    int16_t _dig_P6;
    int16_t _dig_P7;
    int16_t _dig_P8;
    int16_t _dig_P9;
    
    int32_t _t_fine; // Valor intermediário de compensação de temperatura
    
    // Métodos internos de I2C
    bool _readCalibration();
    bool _readRawData(int32_t& adcTemp, int32_t& adcPress);
    bool _readRegister(uint8_t reg, uint8_t& value);
    bool _readRegisters(uint8_t reg, uint8_t* buffer, size_t length);
    bool _writeRegister(uint8_t reg, uint8_t value);
    bool _waitForReady(uint32_t timeoutMs = 100);
    
    // Compensação de dados (datasheet seção 3.11.3)
    int32_t _compensateTemp(int32_t adcTemp);
    uint32_t _compensatePress(int32_t adcPress);
};

#endif // BMP280_H
