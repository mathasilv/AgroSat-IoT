/**
 * @file LoRaHal.cpp
 * @brief Implementação SX1276 HAL Driver (TTGO LoRa32 V2.1)
 * @version 1.0.0
 * @date 2025-11-28
 */

#include "LoRaHal.h"
#include <SPI.h>

namespace Drivers {

LoRaHal::LoRaHal(HAL::SPI* spi, HAL::GPIO* gpio, uint8_t cs, uint8_t rst, uint8_t dio0)
    : _spi(spi), _gpio(gpio), _cs(cs), _rst(rst), _dio0(dio0), _initialized(false) {}

LoRaHal::~LoRaHal() {}

void LoRaHal::writeRegister(uint8_t address, uint8_t value) {
    _gpio->digitalWrite(_cs, HAL::PinState::LOW);
    _spi->transfer(address | 0x80);
    _spi->transfer(value);
    _gpio->digitalWrite(_cs, HAL::PinState::HIGH);
}

uint8_t LoRaHal::readRegister(uint8_t address) {
    _gpio->digitalWrite(_cs, HAL::PinState::LOW);
    _spi->transfer(address & 0x7F);
    uint8_t value = _spi->transfer(0x00);
    _gpio->digitalWrite(_cs, HAL::PinState::HIGH);
    return value;
}

void LoRaHal::writeRegister(uint8_t address, const uint8_t* buffer, size_t size) {
    _gpio->digitalWrite(_cs, HAL::PinState::LOW);
    _spi->transfer(address | 0x80);
    for (size_t i = 0; i < size; i++) {
        _spi->transfer(buffer[i]);
    }
    _gpio->digitalWrite(_cs, HAL::PinState::HIGH);
}

void LoRaHal::readRegister(uint8_t address, uint8_t* buffer, size_t size) {
    _gpio->digitalWrite(_cs, HAL::PinState::LOW);
    _spi->transfer(address & 0x7F);
    for (size_t i = 0; i < size; i++) {
        buffer[i] = _spi->transfer(0x00);
    }
    _gpio->digitalWrite(_cs, HAL::PinState::HIGH);
}

bool LoRaHal::begin(uint32_t frequency) {
    // Configurar pinos
    _gpio->pinMode(_cs, HAL::PinMode::OUTPUT);
    _gpio->pinMode(_rst, HAL::PinMode::OUTPUT);
    _gpio->pinMode(_dio0, HAL::PinMode::INPUT);
    
    // Reset hardware
    _gpio->digitalWrite(_rst, HAL::PinState::LOW);
    delay(10);
    _gpio->digitalWrite(_rst, HAL::PinState::HIGH);
    delay(100);
    
    // Verificar chip ID (RegVersion = 0x12 para SX1276)
    uint8_t version = readRegister(0x42);
    if (version != 0x12) {
        return false;
    }
    
    // Configurar modo LoRa + standby
    writeRegister(0x01, 0x80);  // RegOpMode = Sleep + LoRa
    delay(10);
    writeRegister(0x01, 0x00);  // RegOpMode = Standby
    
    // Configurar frequência
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;
    writeRegister(0x06, (frf >> 16) & 0xFF);  // RegFrfMsb
    writeRegister(0x07, (frf >>  8) & 0xFF);  // RegFrfMid
    writeRegister(0x08, (frf >>  0) & 0xFF);  // RegFrfLsb
    
    _initialized = true;
    return true;
}

void LoRaHal::setTxPower(int8_t power) {
    if (power < 2) power = 2;
    if (power > 20) power = 20;
    
    uint8_t paConfig = 0xFF & (power * 2 + 20);
    writeRegister(0x4D, paConfig);  // RegPaConfig
}

void LoRaHal::setSpreadingFactor(uint8_t sf) {
    if (sf < 6) sf = 6;
    if (sf > 12) sf = 12;
    
    uint8_t modemConfig1 = (readRegister(0x1D) & 0xF1) | ((sf - 4) << 1);  // RegModemConfig1
    writeRegister(0x1D, modemConfig1);
    
    uint8_t modemConfig2 = (readRegister(0x1E) & 0xF8) | (sf > 6 ? 0x04 : 0x00);  // RegModemConfig2
    writeRegister(0x1E, modemConfig2);
}

void LoRaHal::setSignalBandwidth(uint32_t sbw) {
    // Configurar BW (125kHz = 0x70)
    uint8_t modemConfig1 = (readRegister(0x1D) & 0x0F) | 0x70;
    writeRegister(0x1D, modemConfig1);
}

void LoRaHal::setPreambleLength(uint16_t len) {
    writeRegister(0x20, (len >> 8) & 0xFF);  // RegPreambleMsb
    writeRegister(0x21, len & 0xFF);         // RegPreambleLsb
}

void LoRaHal::setSyncWord(uint8_t sw) {
    writeRegister(0x12, sw);  // RegSyncWord
}

void LoRaHal::setCodingRate4(uint8_t cr) {
    if (cr < 5) cr = 5;
    if (cr > 8) cr = 8;
    
    uint8_t crBits = (cr - 1) << 1;
    uint8_t modemConfig1 = (readRegister(0x1D) & 0xF8) | crBits;
    writeRegister(0x1D, modemConfig1);
}

void LoRaHal::enableCrc() {
    uint8_t modemConfig2 = readRegister(0x1E) | 0x04;  // RxPayloadCrcOn
    writeRegister(0x1E, modemConfig2);
}

void LoRaHal::disableCrc() {
    uint8_t modemConfig2 = readRegister(0x1E) & ~0x04;
    writeRegister(0x1E, modemConfig2);
}

void LoRaHal::disableInvertIQ() {
    writeRegister(0x33, 0x27);  // RegInvertIQ = 0x27 (normal)
}

void LoRaHal::beginPacket() {
    writeRegister(0x01, 0x83);  // RegOpMode = FtxMod
}

size_t LoRaHal::write(uint8_t data) {
    writeRegister(0x0C, data);  // RegFifo
    return 1;
}

size_t LoRaHal::write(const uint8_t* buffer, size_t size) {
    uint8_t fifoAddr = readRegister(0x0D);  // RegFifoTxBaseAddr
    writeRegister(0x0D, fifoAddr);          // RegFifoAddrPtr
    writeRegister(0x22, size);              // RegPayloadLength
    
    writeRegister(0x0C, buffer, size);      // RegFifo
    return size;
}

bool LoRaHal::endPacket(bool async) {
    uint8_t timeout = 100;
    while ((readRegister(0x12) & 0x70) != 0x20 && timeout--) {  // TxDone
        delay(10);
    }
    
    writeRegister(0x01, 0x00);  // Standby
    return timeout > 0;
}

int LoRaHal::parsePacket() {
    uint8_t irqFlags = readRegister(0x12);
    if ((irqFlags & 0x40) == 0) return 0;  // RxDone
    
    uint8_t payloadLen = readRegister(0x22);
    writeRegister(0x01, 0x00);  // Standby
    return payloadLen;
}

int LoRaHal::available() {
    return readRegister(0x13);  // RegModemStat RxNbBytes
}

int LoRaHal::read() {
    uint8_t fifoAddr = readRegister(0x10);  // RegFifoRxCurrentAddr
    writeRegister(0x0D, fifoAddr);          // RegFifoAddrPtr
    return readRegister(0x0C);              // RegFifo
}

int LoRaHal::packetRssi() {
    uint8_t rssi = readRegister(0x1A);      // RegPktRssiValue
    return (rssi - 157);  // TTGO LoRa32 offset
}

float LoRaHal::packetSnr() {
    int8_t snr = readRegister(0x1B);        // RegPktSnrValue
    return snr * 0.25f;
}

int LoRaHal::rssi() {
    uint8_t rssi = readRegister(0x1B);      // RegRssiValue
    return (rssi >> 1) - 157;
}

void LoRaHal::receive() {
    writeRegister(0x01, 0x01);  // RegOpMode = RxContinuous
}

void LoRaHal::idle() {
    writeRegister(0x01, 0x00);  // Standby
}

}  // namespace Drivers
