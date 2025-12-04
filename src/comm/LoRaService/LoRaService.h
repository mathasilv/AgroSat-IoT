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
    
    // --- MÉTODOS DE ENVIO ---
    // Envio Binário (Novo - Usado pelo CommunicationManager atualizado)
    bool send(const uint8_t* data, size_t len);
    
    // Envio Texto/Hex (Legado - Mantido para compatibilidade)
    bool send(const String& data);
    
    // --- RECEPÇÃO ---
    bool receive(String& packet, int& rssi, float& snr);
    
    // --- CONTROLE ---
    bool isOnline() const { return _online; }
    void setSpreadingFactor(int sf);
    
    // Controle de Potência Dinâmico (Necessário para a economia de energia)
    void setTxPower(int level);

private:
    LoRaReceiver _receiver;
    LoRaTransmitter _transmitter;
    bool _online;
};

#endif