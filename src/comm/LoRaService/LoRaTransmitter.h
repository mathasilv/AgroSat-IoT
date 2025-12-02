#ifndef LORA_TRANSMITTER_H
#define LORA_TRANSMITTER_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

class LoRaTransmitter {
public:
    LoRaTransmitter();
    bool send(const String& data);
    void setSpreadingFactor(int sf);

private:
    int _currentSF;
    bool _isChannelFree();
};

#endif