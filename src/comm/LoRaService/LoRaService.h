/**
 * @file LoRaService.h
 * @brief Serviço LoRa Limpo (Apenas Binary Mode)
 */

#ifndef LORASERVICE_H
#define LORASERVICE_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"
#include "DutyCycleTracker.h"

class LoRaService {
public:
    LoRaService();
    bool begin();
    
    // Envio: Mantido apenas o método binário (mais eficiente e seguro)
    // Se precisar enviar texto, converta para (uint8_t*)
    bool send(const uint8_t* data, size_t len, bool isAsync = false);
    
    // Recepção
    bool receive(String& packet, int& rssi, float& snr);

    void setTxPower(int level);
    void setSpreadingFactor(int sf);
    
    int getCurrentSF() const { return _currentSF; }
    int getLastRSSI() const { return _lastRSSI; }
    float getLastSNR() const { return _lastSNR; }
    
    DutyCycleTracker& getDutyCycleTracker() { return _dutyCycle; }
    bool canTransmitNow(uint32_t payloadSize);

    static void onDio0Rise(int packetSize);

private:
    int _currentSF;
    int _lastRSSI;
    float _lastSNR;
    DutyCycleTracker _dutyCycle;
    static volatile int _rxPacketSize;
};

#endif