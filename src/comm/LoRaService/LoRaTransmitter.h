#ifndef LORA_TRANSMITTER_H
#define LORA_TRANSMITTER_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

class LoRaTransmitter {
public:
    LoRaTransmitter();
    bool send(const uint8_t* data, size_t len); // Novo método binário
    bool send(const String& data);              // Legado
    void setSpreadingFactor(int sf);

private:
    int _currentSF;
    bool _isChannelFree();
};

#endif