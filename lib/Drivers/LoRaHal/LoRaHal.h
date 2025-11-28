/**
 * @file LoRaHal.h
 * @brief SX1276 LoRa HAL Driver para AgroSat-IoT (TTGO LoRa32 V2.1)
 * @version 1.0.0
 * @date 2025-11-28
 */

#ifndef LORAHAL_H
#define LORAHAL_H

#include "../../HAL/interface/SPI.h"
#include "../../HAL/interface/GPIO.h"
#include "../../../include/config.h"
#include <Arduino.h>

namespace Drivers {

class LoRaHal {
private:
    HAL::SPI* _spi;
    HAL::GPIO* _gpio;
    uint8_t _cs, _rst, _dio0;
    bool _initialized;
    
    void writeRegister(uint8_t address, uint8_t value);
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, const uint8_t* buffer, size_t size);
    void readRegister(uint8_t address, uint8_t* buffer, size_t size);

public:
    LoRaHal(HAL::SPI* spi, HAL::GPIO* gpio, 
            uint8_t cs = LORA_CS, uint8_t rst = LORA_RST, uint8_t dio0 = LORA_DIO0);
    ~LoRaHal();
    
    // Inicialização
    bool begin(uint32_t frequency);
    
    // Configurações RF
    void setTxPower(int8_t power);
    void setSpreadingFactor(uint8_t sf);
    void setSignalBandwidth(uint32_t sbw);
    void setPreambleLength(uint16_t len);
    void setSyncWord(uint8_t sw);
    void setCodingRate4(uint8_t cr);
    void enableCrc();
    void disableCrc();
    void disableInvertIQ();
    
    // Transmissão
    void beginPacket();
    size_t write(const uint8_t* buffer, size_t size);
    size_t write(uint8_t data);
    bool endPacket(bool async = false);
    
    // Recepção
    int parsePacket();
    int available();
    int read();
    int packetRssi();
    float packetSnr();
    int rssi();
    
    // Modos
    void receive();
    void idle();
    bool isOnline() const { return _initialized; }
};

}  // namespace Drivers

#endif // LORAHAL_H
