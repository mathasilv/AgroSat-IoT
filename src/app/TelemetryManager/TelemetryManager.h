/**
 * @file TelemetryManager.h
 * @brief Gerenciador central de telemetria do sistema AgroSat-IoT
 * 
 * @details Classe principal que orquestra todos os subsistemas do dispositivo:
 *          sensores, GPS, comunicação, armazenamento e controle de missão.
 *          Implementa máquina de estados para modos de operação.
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 3.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Subsistemas Gerenciados
 * | Subsistema           | Classe                | Função                      |
 * |----------------------|-----------------------|-----------------------------|
 * | Sensores Ambientais  | SensorManager         | IMU, Pressão, Umidade, CO2  |
 * | Posicionamento       | GPSManager            | Coordenadas e altitude GPS  |
 * | Energia              | PowerManager          | Bateria e modo sleep        |
 * | Comunicação          | CommunicationManager  | LoRa, WiFi, HTTP            |
 * | Armazenamento        | StorageManager        | SD Card logging             |
 * | Controle de Missão   | MissionController     | Estados e transições        |
 * 
 * ## Modos de Operação
 * - **PREFLIGHT**: Inicialização e verificações
 * - **FLIGHT**: Coleta ativa de telemetria
 * - **SAFE**: Modo de emergência/baixa energia
 * 
 * @see MissionController para detalhes da máquina de estados
 * @see TelemetryCollector para coleta de dados
 */

#ifndef TELEMETRYMANAGER_H
#define TELEMETRYMANAGER_H

#include <Arduino.h>
#include "config.h"

// Subsistemas
#include "sensors/SensorManager/SensorManager.h"
#include "sensors/GPSManager/GPSManager.h"
#include "core/PowerManager/PowerManager.h"
#include "core/SystemHealth/SystemHealth.h"
#include "core/RTCManager/RTCManager.h"
#include "core/ButtonHandler/ButtonHandler.h"
#include "storage/StorageManager.h"
#include "comm/CommunicationManager/CommunicationManager.h"
#include "app/GroundNodeManager/GroundNodeManager.h"
#include "app/MissionController/MissionController.h"
#include "app/TelemetryCollector/TelemetryCollector.h"
#include "core/CommandHandler/CommandHandler.h"

/**
 * @class TelemetryManager
 * @brief Orquestrador central de todos os subsistemas de telemetria
 */
class TelemetryManager {
public:
    //=========================================================================
    // CICLO DE VIDA
    //=========================================================================
    
    /**
     * @brief Construtor padrão
     */
    TelemetryManager();
    
    /**
     * @brief Inicializa todos os subsistemas
     * @return true se inicialização bem sucedida
     * @return false se falha crítica em subsistema essencial
     */
    bool begin();
    
    /**
     * @brief Loop principal - chamado continuamente
     * @details Gerencia comunicação rádio, rede de ground nodes,
     *          envio de telemetria e verificações operacionais.
     */
    void loop();
    
    /**
     * @brief Atualiza sensores físicos (chamado pela SensorsTask)
     * @note Thread-safe - usa mutexes internamente
     */
    void updatePhySensors();
    
    /**
     * @brief Processa comando recebido via Serial
     * @param cmd Comando em formato string (case-insensitive)
     * @return true se comando reconhecido e executado
     * @return false se comando desconhecido
     */
    bool handleCommand(const String& cmd);
    
    /**
     * @brief Alimenta o watchdog timer
     * @note Deve ser chamado periodicamente para evitar reset
     */
    void feedWatchdog();

    //=========================================================================
    // CONTROLE DE MISSÃO
    //=========================================================================
    
    /**
     * @brief Inicia missão (transição para modo FLIGHT)
     */
    void startMission();
    
    /**
     * @brief Para missão (retorna ao modo PREFLIGHT)
     */
    void stopMission();
    
    /**
     * @brief Retorna modo de operação atual
     * @return OperationMode enum (PREFLIGHT, FLIGHT, SAFE)
     */
    OperationMode getMode() { return _mode; }
    
    /**
     * @brief Aplica configurações específicas de um modo
     * @param modeIndex Índice do modo (0=PREFLIGHT, 1=FLIGHT, 2=SAFE)
     */
    void applyModeConfig(uint8_t modeIndex);

    //=========================================================================
    // PROCESSAMENTO DE FILAS (TASKS)
    //=========================================================================
    
    /**
     * @brief Processa pacote da fila HTTP (chamado pela HttpTask)
     * @param msg Mensagem da fila HTTP
     */
    void processHttpPacket(const HttpQueueMessage& msg) { _comm.processHttpQueuePacket(msg); }
    
    /**
     * @brief Processa pacote da fila Storage (chamado pela StorageTask)
     * @param msg Mensagem da fila de armazenamento
     */
    void processStoragePacket(const StorageQueueMessage& msg);

private:
    //=========================================================================
    // SUBSISTEMAS
    //=========================================================================
    SensorManager        _sensors;            ///< Gerenciador de sensores I2C
    GPSManager           _gps;                ///< Gerenciador de GPS
    PowerManager         _power;              ///< Gerenciador de energia
    SystemHealth         _systemHealth;       ///< Monitor de saúde do sistema
    RTCManager           _rtc;                ///< Gerenciador de RTC
    ButtonHandler        _button;             ///< Handler de botões
    StorageManager       _storage;            ///< Gerenciador de SD Card
    CommunicationManager _comm;               ///< Gerenciador de comunicação
    GroundNodeManager    _groundNodes;        ///< Gerenciador de ground nodes
    MissionController    _mission;            ///< Controlador de missão
    TelemetryCollector   _telemetryCollector; ///< Coletor de telemetria
    CommandHandler       _commandHandler;     ///< Handler de comandos
    
    //=========================================================================
    // ESTADO
    //=========================================================================
    OperationMode  _mode;                     ///< Modo de operação atual
    TelemetryData  _telemetryData;            ///< Buffer de dados de telemetria

    //=========================================================================
    // TIMESTAMPS
    //=========================================================================
    unsigned long  _lastTelemetrySend;        ///< Último envio de telemetria
    unsigned long  _lastStorageSave;          ///< Última gravação em SD
    unsigned long  _lastBeaconTime;           ///< Último beacon enviado

    //=========================================================================
    // MÉTODOS PRIVADOS - INICIALIZAÇÃO
    //=========================================================================
    void _initModeDefaults();                                        ///< Configura defaults do modo
    void _initSubsystems(uint8_t& subsystemsOk, bool& success);      ///< Inicializa subsistemas
    void _syncNTPIfAvailable();                                      ///< Sincroniza RTC via NTP
    void _logInitSummary(bool success, uint8_t subsystemsOk, uint32_t initialHeap); ///< Log de init
    
    //=========================================================================
    // MÉTODOS PRIVADOS - LOOP
    //=========================================================================
    void _handleIncomingRadio();        ///< Processa pacotes LoRa recebidos
    void _maintainGroundNetwork();      ///< Mantém rede de ground nodes
    void _sendTelemetry();              ///< Envia telemetria via LoRa
    void _saveToStorage();              ///< Salva dados no SD Card
    void _checkOperationalConditions(); ///< Verifica condições operacionais
    void _handleButtonEvents();         ///< Processa eventos de botão
    void _updateLEDIndicator(unsigned long currentTime); ///< Atualiza LED de status
    void _sendSafeBeacon();             ///< Envia beacon em modo SAFE
};

#endif