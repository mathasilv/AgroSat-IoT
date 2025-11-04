/**
 * @file PayloadManager.h
 * @brief Gerenciamento da missão AgroSat-IoT (LoRa)
 * @version 1.0.0
 * 
 * Responsável por:
 * - Recepção de dados via LoRa dos sensores terrestres
 * - Processamento de dados agrícolas
 * - Geração do payload customizado (máximo 90 bytes)
 * - Estatísticas de comunicação LoRa
 */

#ifndef PAYLOAD_MANAGER_H
#define PAYLOAD_MANAGER_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

class PayloadManager {
public:
    /**
     * @brief Construtor
     */
    PayloadManager();
    
    /**
     * @brief Inicializa módulo LoRa
     * @return true se inicializado com sucesso
     */
    bool begin();
    
    /**
     * @brief Atualiza e processa pacotes LoRa recebidos
     */
    void update();
    
    /**
     * @brief Retorna dados da missão
     */
    MissionData getMissionData();
    
    /**
     * @brief Gera payload customizado para telemetria (máx 90 bytes)
     */
    String generatePayload();
    
    /**
     * @brief Envia comando via LoRa (para estações terrestres)
     */
    bool sendCommand(const String& command);
    
    /**
     * @brief Retorna status do módulo LoRa
     */
    bool isOnline();
    
    /**
     * @brief Retorna RSSI do último pacote recebido
     */
    int16_t getLastRSSI();
    
    /**
     * @brief Retorna SNR do último pacote recebido
     */
    float getLastSNR();
    
    /**
     * @brief Retorna estatísticas de comunicação
     */
    void getStatistics(uint16_t& received, uint16_t& lost, uint16_t& sent);
    
    /**
     * @brief Configura parâmetros LoRa
     */
    void setSpreadingFactor(uint8_t sf);
    void setBandwidth(long bandwidth);
    void setTxPower(uint8_t power);

private:
    bool _online;
    MissionData _missionData;
    
    // Controle de comunicação
    uint32_t _lastPacketTime;
    uint32_t _packetTimeout;
    uint16_t _expectedSeqNum;
    
    /**
     * @brief Processa pacote LoRa recebido
     */
    bool _processPacket(String packet);
    
    /**
     * @brief Valida checksum do pacote
     */
    bool _validateChecksum(const String& packet);
    
    /**
     * @brief Extrai dados do pacote (formato customizado)
     */
    bool _parseAgroData(const String& data);
};

#endif // PAYLOAD_MANAGER_H
