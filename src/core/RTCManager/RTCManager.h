/**
 * @file RTCManager.h
 * @brief Gerenciador de tempo com RTC DS3231 e sincronização NTP
 * 
 * @details Sistema de gerenciamento de tempo com suporte a:
 *          - RTC DS3231 de alta precisão (±2ppm)
 *          - Sincronização via NTP quando WiFi disponível
 *          - Detecção de perda de energia do RTC
 *          - Conversão entre horário local e UTC
 *          - Timestamp Unix para logging
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Especificações do DS3231
 * | Parâmetro       | Valor           |
 * |-----------------|----------------|
 * | Precisão        | ±2 ppm         |
 * | Interface       | I2C (0x68)     |
 * | Backup          | Bateria CR2032 |
 * | Faixa de temp   | -40°C a +85°C  |
 * 
 * ## Hierarquia de Sincronização
 * 1. **NTP** (se WiFi disponível) - Mais preciso
 * 2. **DS3231** (backup) - Mantém hora offline
 * 3. **millis()** (fallback) - Último recurso
 * 
 * @note Fuso horário configurado: UTC-3 (Brasília)
 * @note Endereço I2C do DS3231: 0x68
 */

#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include "esp_sntp.h"

/**
 * @class RTCManager
 * @brief Gerenciador de RTC DS3231 com suporte a NTP
 */
class RTCManager {
public:
    /**
     * @brief Construtor padrão
     */
    RTCManager();
    
    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Inicializa o RTC DS3231
     * @param wire Ponteiro para instância TwoWire (default: Wire)
     * @return true se RTC detectado e inicializado
     * @return false se falha na comunicação I2C
     */
    bool begin(TwoWire* wire = &Wire);
    
    /**
     * @brief Atualiza estado interno (verificações periódicas)
     */
    void update();

    //=========================================================================
    // SINCRONIZAÇÃO
    //=========================================================================
    
    /**
     * @brief Sincroniza RTC com servidor NTP
     * @return true se sincronização bem sucedida
     * @return false se WiFi desconectado ou timeout
     * @note Requer WiFi conectado
     */
    bool syncWithNTP();
    
    //=========================================================================
    // STATUS
    //=========================================================================
    
    /** @brief RTC inicializado e funcionando? */
    bool isInitialized() const;
    
    //=========================================================================
    // GETTERS DE TEMPO
    //=========================================================================
    
    /**
     * @brief Retorna data/hora local formatada
     * @return String no formato "YYYY-MM-DD HH:MM:SS"
     */
    String getDateTime();
    
    /**
     * @brief Retorna data/hora UTC formatada
     * @return String no formato "YYYY-MM-DD HH:MM:SS"
     */
    String getUTCDateTime();
    
    /**
     * @brief Retorna timestamp Unix (segundos desde 1970)
     * @return uint32_t epoch time em UTC
     */
    uint32_t getUnixTime();
    
    /**
     * @brief Retorna objeto DateTime do RTClib
     * @return DateTime com data/hora atual
     */
    DateTime getNow();

    /** @brief Alias para getDateTime() (compatibilidade) */
    String getLocalDateTime() { return getDateTime(); } 

private:
    //=========================================================================
    // HARDWARE
    //=========================================================================
    RTC_DS3231 _rtc;             ///< Instância do driver DS3231
    TwoWire* _wire;              ///< Ponteiro para barramento I2C
    
    //=========================================================================
    // ESTADO
    //=========================================================================
    bool _initialized;           ///< RTC inicializado?
    bool _lost_power;            ///< RTC perdeu energia?
    
    //=========================================================================
    // CONFIGURAÇÕES
    //=========================================================================
    static constexpr uint8_t DS3231_ADDR = 0x68;           ///< Endereço I2C
    static constexpr long GMT_OFFSET_SEC = -3 * 3600;      ///< UTC-3 (Brasília)
    static constexpr int DAYLIGHT_OFFSET_SEC = 0;          ///< Sem horário de verão
    static constexpr const char* NTP_SERVER_1 = "pool.ntp.org";   ///< NTP primário
    static constexpr const char* NTP_SERVER_2 = "time.nist.gov";  ///< NTP secundário
    
    //=========================================================================
    // MÉTODOS PRIVADOS
    //=========================================================================
    
    /** @brief Detecta presença do DS3231 no barramento I2C */
    bool _detectRTC();
    
    /** @brief Sincroniza relógio do sistema com RTC */
    void _syncSystemToRTC();
};

#endif