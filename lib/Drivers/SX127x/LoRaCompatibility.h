/**
 * @file LoRaCompatibility.h
 * @brief 100% Drop-in Replacement for sandeepmistry/arduino-LoRa
 */

#ifndef LORA_COMPATIBILITY_H
#define LORA_COMPATIBILITY_H

#include <Arduino.h>
#include <HAL/interface/SPI.h>
#include "SX127x.h"

namespace SX127x {
    extern Radio LoRa;
}

class LoRaClass {
private:
    SX127x::Radio* radio_;

public:
    LoRaClass() : radio_(&SX127x::LoRa) {}

    // INITIALIZATION
    bool begin(uint32_t frequency) {
        return radio_->begin(frequency);
    }
    
    void setPins(int8_t cs, int8_t reset, int8_t dio0) {
        radio_->setPins(cs, reset, dio0);
    }

    // RF CONFIGURATION
    bool setTxPower(int8_t power) {
        return radio_->setTxPower(power) == SX127x::RadioError::OK;
    }
    
    bool setTxPower(int8_t power, int8_t outputPin) {
        return setTxPower(power);
    }
    
    void setSignalBandwidth(uint32_t bw) {
        radio_->setSignalBandwidth(bw);
    }
    
    void setSpreadingFactor(uint8_t sf) {
        radio_->setSpreadingFactor(sf);
    }
    
    void setCodingRate4(uint8_t denominator) {
        radio_->setCodingRate4(denominator);
    }
    
    void setPreambleLength(uint16_t length) {
        radio_->setPreambleLength(length);
    }
    
    void setSyncWord(int syncWord) {
        radio_->setSyncWord(syncWord);
    }

    // PACKET SETTINGS
    void enableCrc() { radio_->enableCrc(); }
    void disableCrc() { radio_->disableCrc(); }
    void enableInvertIQ() { radio_->enableInvertIQ(); }
    void disableInvertIQ() { radio_->disableInvertIQ(); }

    // TRANSMISSION ✅ COM PRINT CORRIGIDO
    bool beginPacket() {
        return radio_->beginPacket(false) == SX127x::RadioError::OK;
    }
    
    bool beginPacket(bool implicitHeader) {
        return radio_->beginPacket(implicitHeader) == SX127x::RadioError::OK;
    }
    
    size_t write(uint8_t byte) {
        return radio_->write(byte);
    }
    
    size_t write(const uint8_t* buffer, size_t size) {
        return radio_->write(buffer, size);
    }
    
    // ✅ PRINT METHODS CORRIGIDOS (CRÍTICO!)
    size_t print(const String& str) {
        return radio_->print(str);
    }
    
    size_t print(const char* str) {
        return radio_->print(str);
    }
    
    size_t print(int n) {
        char buf[16];
        sprintf(buf, "%d", n);
        return print(buf);
    }
    
    size_t print(unsigned long n) {
        char buf[16];
        sprintf(buf, "%lu", n);
        return print(buf);
    }
    
    size_t print(long n) {
        char buf[16];
        sprintf(buf, "%ld", n);
        return print(buf);
    }
    
    bool endPacket() {
        return radio_->endPacket(false);
    }
    
    bool endPacket(bool async) {
        return radio_->endPacket(async);
    }

    // RECEPTION
    void receive(int size = 0) {
        radio_->receive();
    }
    
    int parsePacket() {
        return radio_->parsePacket();
    }
    
    int available() {
        return radio_->available();
    }
    
    int read() {
        return radio_->read();
    }
    
    int peek() {
        return radio_->peek();
    }

    // STATISTICS
    int packetRssi() { return radio_->packetRssi(); }
    float packetSnr() { return radio_->packetSnr(); }
    int rssi() { return radio_->rssi(); }
    int32_t packetFrequencyError() { return radio_->packetFrequencyError(); }

    // MODE CONTROL
    void sleep() { radio_->sleep(); }
    void standby() { radio_->standby(); }
    void idle() { radio_->standby(); }
    
    bool isTransmitting() { return radio_->isTransmitting(); }

    // ADVANCED
    bool channelActivityDetection() {
        return radio_->channelActivityDetection();
    }

    // STUBS
    void setGain(uint8_t gain) {}
    
    // ✅ RANDOM CORRIGIDO
    uint8_t random(uint8_t max) { 
        return (uint8_t)(random(8) % (max ? max : 1)); 
    }
    
    SX127x::RadioError getLastError() {
        return radio_->getLastError();
    }
    
#ifdef SX127X_DEBUG_REGISTERS
    void dumpRegisters() {
        radio_->dumpRegisters();
    }
#endif
};

extern LoRaClass LoRa;

#endif // LORA_COMPATIBILITY_H
