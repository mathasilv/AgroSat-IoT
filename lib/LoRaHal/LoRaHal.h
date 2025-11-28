#ifndef LORAHAL_H
#define LORAHAL_H

#include "HAL/interface/SPI.h"
#include "HAL/interface/GPIO.h"
#include <cstdint>
#include <cstddef>

#define LORA_DEFAULT_SS 18
#define LORA_DEFAULT_RESET 14
#define LORA_DEFAULT_DIO0 26

#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_LR_OCP 0x0B
#define REG_LR_PREAMBLE_MSB 0x20
#define REG_LR_PREAMBLE_LSB 0x21
#define REG_LR_PAYLOAD_LENGTH 0x22
#define REG_LR_MODEM_CONFIG_1 0x26
#define REG_LR_MODEM_CONFIG_2 0x27
#define REG_LR_SYNC_WORD 0x2C
#define REG_LR_FIFO_RX_BYTE_ADDR 0x2D
#define REG_LR_IRQ_FLAGS 0x12
#define REG_LR_IRQ_FLAGS_MASK 0x11
#define REG_LR_FIFO_RX_CURRENT_ADDR 0x25
#define REG_LR_MODEM_CONFIG_3 0x28
#define REG_LR_INVERT_IQ 0x33

#define PA_OUTPUT_RFO_PIN 1
#define PA_OUTPUT_PA_BOOST_PIN 0

class LoRaHal {
private:
    HAL::SPI* _spi;
    HAL::GPIO* _gpio;
    uint8_t _cs, _rst, _dio0;
    bool _dio0Irq;
    
    void writeRegister(uint8_t address, uint8_t value);
    void writeRegister(uint8_t address, const uint8_t* buffer, uint8_t size);
    uint8_t readRegister(uint8_t address);
    void readRegister(uint8_t address, uint8_t* buffer, uint8_t size);
    void reset();
    void explicitHeaderMode();
    uint8_t readRegister(uint8_t address, uint8_t second) {
        uint8_t buffer[2] = {address, second};
        writeRegister(0x80 | address, buffer, 1);
        return readRegister(address);
    }

public:
    LoRaHal(HAL::SPI* spi, HAL::GPIO* gpio, uint8_t cs=LORA_DEFAULT_SS, 
            uint8_t rst=LORA_DEFAULT_RESET, uint8_t dio0=LORA_DEFAULT_DIO0);
    
    ~LoRaHal();
    
    // EXATAS funções do CommunicationManager
    bool begin(uint32_t frequency);
    void setTxPower(int8_t power, int outputPin=PA_OUTPUT_PA_BOOST_PIN);
    void setSpreadingFactor(uint8_t sf);
    void setSignalBandwidth(uint32_t sbw);
    void setPreambleLength(uint16_t len);
    void setSyncWord(uint8_t sw);
    void setCodingRate4(uint8_t denominator);
    void enableCrc();
    void disableCrc();
    void disableInvertIQ();
    
    bool beginPacket();
    int endPacket(bool async=false);
    size_t write(const uint8_t* buffer, size_t size);
    size_t write(uint8_t data) { return write(&data, 1); }
    
    int parsePacket();
    int available();
    int read();
    int packetRssi();
    float packetSnr();
    int rssi();
    void receive();
};

#endif
