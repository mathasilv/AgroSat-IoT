/**
 * @file LoRaService.h
 * @brief Serviço de comunicação LoRa para telemetria de longo alcance
 * 
 * @details Implementa comunicação LoRa usando o transceiver SX1276/SX1278.
 *          Suporta transmissão binária com controle de duty cycle para
 *          conformidade com regulamentações ISM (1% duty cycle).
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Características
 * - Transmissão binária otimizada
 * - Recepção por interrupção (DIO0)
 * - Controle automático de duty cycle
 * - Cálculo de Time-on-Air
 * - Spreading Factor adaptativo
 * 
 * ## Configuração Padrão
 * - Frequência: 915 MHz (configurável)
 * - SF: 7-12 (adaptativo)
 * - BW: 125 kHz
 * - CR: 4/5
 * 
 * @see DutyCycleTracker para controle de duty cycle
 * @note Requer biblioteca LoRa by Sandeep Mistry
 */

#ifndef LORASERVICE_H
#define LORASERVICE_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"
#include "DutyCycleTracker.h"

/**
 * @class LoRaService
 * @brief Gerenciador de comunicação LoRa com suporte a interrupções
 */
class LoRaService {
public:
    /**
     * @brief Construtor padrão
     */
    LoRaService();
    
    /**
     * @brief Inicializa o hardware LoRa e configura interrupções
     * @return true se SX1276/78 detectado e configurado
     * @return false se falha na comunicação SPI
     */
    bool begin();
    
    //=========================================================================
    // TRANSMISSÃO
    //=========================================================================
    
    /**
     * @brief Envia dados binários via LoRa
     * 
     * @param data Ponteiro para buffer de dados
     * @param len Tamanho dos dados em bytes
     * @param isAsync Se true, não aguarda fim da transmissão
     * @return true sempre (transmissão iniciada)
     * 
     * @note Para enviar texto, converta: send((uint8_t*)str, strlen(str))
     * @warning Verificar canTransmitNow() antes para respeitar duty cycle
     */
    bool send(const uint8_t* data, size_t len, bool isAsync = false);
    
    //=========================================================================
    // RECEPÇÃO
    //=========================================================================
    
    /**
     * @brief Verifica e lê pacote recebido (non-blocking)
     * 
     * @param[out] packet String com dados recebidos
     * @param[out] rssi RSSI do pacote em dBm
     * @param[out] snr SNR do pacote em dB
     * @return true se pacote disponível e lido
     * @return false se nenhum pacote pendente
     * 
     * @note Usa semáforo para sincronização com ISR
     */
    bool receive(String& packet, int& rssi, float& snr);

    //=========================================================================
    // CONFIGURAÇÃO
    //=========================================================================
    
    /**
     * @brief Define potência de transmissão
     * @param level Potência em dBm (2-20)
     */
    void setTxPower(int level);
    
    /**
     * @brief Define Spreading Factor
     * @param sf Valor de SF (7-12)
     * @note SF maior = maior alcance, menor taxa de dados
     */
    
    /**
     * @brief Acesso ao tracker de duty cycle
     * @return Referência ao DutyCycleTracker interno
     */
    DutyCycleTracker& getDutyCycleTracker() { return _dutyCycle; }
    
    /**
     * @brief Verifica se pode transmitir respeitando duty cycle
     * @param payloadSize Tamanho do payload em bytes
     * @return true se dentro do limite de 1% duty cycle
     * @return false se deve aguardar
     */
    bool canTransmitNow(uint32_t payloadSize);

    /**
     * @brief Callback de interrupção DIO0 (pacote recebido)
     * @param packetSize Tamanho do pacote recebido
     * @note Função estática chamada pela ISR - IRAM_ATTR
     */
    static void onDio0Rise(int packetSize);

private:
    //=========================================================================
    // VARIÁVEIS PRIVADAS
    //=========================================================================
    int _currentSF;                      ///< Spreading Factor atual
    int _lastRSSI;                       ///< RSSI do último RX (dBm)
    float _lastSNR;                      ///< SNR do último RX (dB)
    DutyCycleTracker _dutyCycle;         ///< Controlador de duty cycle
    
    static volatile int _rxPacketSize;   ///< Tamanho do pacote RX (ISR)
};

#endif