/**
 * @file LoRaService.h
 * @brief Serviço de rádio LoRa (baixo nível + duty cycle/CAD)
 * @version 1.0.0
 * @date 2025-12-01
 */

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "config.h"

class LoRaService {
public:
    LoRaService();

    /**
     * @brief Inicializa o módulo LoRa e aplica os parâmetros padrão.
     * @return true se inicializou com sucesso.
     */
    bool begin();

    /**
     * @brief Inicializa apenas o hardware LoRa (reset, SPI, LoRa.begin).
     */
    bool init();

    /**
     * @brief Tenta reinicializar o LoRa algumas vezes.
     */
    bool retryInit(uint8_t maxAttempts = 3);

    /**
     * @brief Habilita/desabilita o uso de LoRa (flag de software).
     */
    void enable(bool enable);

    /**
     * @brief Envia um payload LoRa respeitando duty cycle, CAD e timeout.
     */
    bool send(const String& data);

    /**
     * @brief Tenta receber um pacote LoRa, aplicando filtros de RSSI/SNR.
     */
    bool receive(String& packet, int& rssi, float& snr);

    /**
     * @brief Indica se o rádio LoRa está inicializado.
     */
    bool isOnline() const;

    /**
     * @brief Último RSSI válido medido em um pacote recebido.
     */
    int getLastRSSI() const;

    /**
     * @brief Último SNR válido medido em um pacote recebido.
     */
    float getLastSNR() const;

    /**
     * @brief Estatísticas de envio LoRa (sucesso/falha).
     */
    void getStatistics(uint16_t& sent, uint16_t& failed) const;

    /**
     * @brief Ajusta SF/potência conforme modo de operação (PREFLIGHT/FLIGHT/SAFE).
     */
    void reconfigure(OperationMode mode);

    /**
     * @brief Ajusta SF dinamicamente em função da altitude.
     */
    void adaptSpreadingFactor(float altitude);

private:
    // Configuração interna
    void _configureParameters();
    bool _validatePayloadSize(size_t size) const;
    bool _isChannelFree() const;

    // Estado LoRa
    bool          _initialized;
    bool          _enabled;
    int           _lastRSSI;
    float         _lastSNR;
    uint16_t      _packetsSent;
    uint16_t      _packetsFailed;
    unsigned long _lastTx;
    uint8_t       _currentSpreadingFactor;
    uint8_t       _txFailureCount;
    unsigned long _lastTxFailure;
};
