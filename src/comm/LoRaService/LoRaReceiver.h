#ifndef LORA_RECEIVER_H
#define LORA_RECEIVER_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

class LoRaReceiver {
public:
    LoRaReceiver();
    
    // Retorna true se um pacote foi recebido e processado
    bool receive(String& packet, int& rssi, float& snr);
    
    int getLastRSSI() const { return _lastRSSI; }
    float getLastSNR() const { return _lastSNR; }

private:
    int _lastRSSI;
    float _lastSNR;
};

#endif