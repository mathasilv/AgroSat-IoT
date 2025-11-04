/**
 * @file TelemetryManager.h
 * @brief Gerenciador central de telemetria - Coordenação robusta de todos subsistemas
 * @version 1.1.0
 * 
 * Responsável por:
 * - Coordenação de todos os subsistemas
 * - Coleta de dados de telemetria
 * - Agendamento de envios
 * - Display OLED com informações
 * - Modo de operação (pré-voo, voo, pós-voo)
 * - Monitoramento de heap e robustez
 */

#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include <Arduino.h>
#include <SSD1306Wire.h>
#include "config.h"
#include "PowerManager.h"
#include "SensorManager.h"
#include "CommunicationManager.h"
#include "StorageManager.h"
#include "PayloadManager.h"
#include "SystemHealth.h"

enum OperationMode {
    MODE_INIT,          // Inicialização
    MODE_PREFLIGHT,     // Pré-voo (testes, calibração)
    MODE_FLIGHT,        // Voo (operação normal)
    MODE_POSTFLIGHT,    // Pós-voo (finalização)
    MODE_ERROR          // Modo de erro
};

class TelemetryManager {
public:
    /**
     * @brief Construtor
     */
    TelemetryManager();
    
    /**
     * @brief Inicializa todos os subsistemas
     * @return true se todos inicializados
     */
    bool begin();
    
    /**
     * @brief Loop principal de telemetria
     */
    void loop();
    
    /**
     * @brief Inicia missão (modo voo)
     */
    void startMission();
    
    /**
     * @brief Para missão
     */
    void stopMission();
    
    /**
     * @brief Retorna modo de operação atual
     */
    OperationMode getMode();
    
    /**
     * @brief Atualiza display OLED
     */
    void updateDisplay();

private:
    // Subsistemas
    PowerManager _power;
    SensorManager _sensors;
    CommunicationManager _comm;
    StorageManager _storage;
    PayloadManager _payload;
    SystemHealth _health;
    SSD1306Wire _display;
    
    // Dados
    TelemetryData _telemetryData;
    MissionData _missionData;
    
    // Controle de operação
    OperationMode _mode;
    uint32_t _lastTelemetrySend;
    uint32_t _lastStorageSave;
    uint32_t _lastDisplayUpdate;
    
    // Monitoramento de heap (NOVOS CAMPOS)
    uint32_t _lastHeapCheck;
    uint32_t _minHeapSeen;
    
    /**
     * @brief Coleta dados de todos os sensores
     */
    void _collectTelemetryData();
    
    /**
     * @brief Envia telemetria via WiFi
     */
    void _sendTelemetry();
    
    /**
     * @brief Salva dados no SD Card
     */
    void _saveToStorage();
    
    /**
     * @brief Verifica condições de operação
     */
    void _checkOperationalConditions();
    
    /**
     * @brief Exibe informações no display
     */
    void _displayStatus();
    void _displayTelemetry();
    void _displayError(const String& error);
    
    /**
     * @brief Métodos de monitoramento de heap (NOVOS MÉTODOS)
     */
    void _logHeapUsage(const String& component);
    void _monitorHeap();
};

#endif // TELEMETRY_MANAGER_H
