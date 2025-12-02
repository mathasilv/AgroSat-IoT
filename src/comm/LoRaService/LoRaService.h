#ifndef LORA_SERVICE_H
#define LORA_SERVICE_H

#include <Arduino.h>
#include "LoRaReceiver.h"
#include "LoRaTransmitter.h"
#include "config.h"

class LoRaService {
public:
    LoRaService();
    
    bool begin();
    
    bool send(const String& data);
    bool receive(String& packet, int& rssi, float& snr);
    
    bool isOnline() const { return _online; }
    void setSpreadingFactor(int sf);

private:
    LoRaReceiver _receiver;
    LoRaTransmitter _transmitter;
    bool _online;
};

#endif