/**
 * @file SystemHealth.h
 * @brief Monitoramento de saúde e watchdog do sistema
 * @version 1.1.0
 *
 * Responsável por:
 * - Watchdog timer (reset automático em caso de travamento)
 * - Monitoramento de temperatura interna
 * - Detecção de anomalias
 * - Status de subsistemas
 * - Logs de eventos críticos
 * - Monitoramento de heap (baixa memória)
 */

#ifndef SYSTEM_HEALTH_H
#define SYSTEM_HEALTH_H

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"

class SystemHealth {
public:
    /**
     * @brief Construtor
     */
    SystemHealth();

    /**
     * @brief Status de heap monitorado pelo SystemHealth
     */
    enum class HeapStatus {
        OK,
        LOW_HEAP,       ///< Alerta: memória baixa
        CRITICAL_HEAP,  ///< Recomenda entrar em SAFE MODE
        FATAL_HEAP      ///< Recomenda reiniciar o sistema
    };

    /**
     * @brief Inicializa watchdog e monitoramento
     * @return true se inicializado com sucesso
     */
    bool begin();

    /**
     * @brief Atualiza status do sistema (chamada no loop)
     */
    void update();

    /**
     * @brief Reseta watchdog timer (feed the dog)
     */
    void feedWatchdog();

    /**
     * @brief Registra erro no sistema
     */
    void reportError(uint8_t errorCode, const String& description);

    /**
     * @brief Retorna temperatura interna da CPU (°C)
     */
    float getCPUTemperature();

    /**
     * @brief Retorna uptime do sistema (millis)
     */
    unsigned long getUptime();

    /**
     * @brief Retorna tempo da missão (millis desde início)
     */
    unsigned long getMissionTime();

    /**
     * @brief Retorna memória livre (bytes)
     */
    uint32_t getFreeHeap();

    /**
     * @brief Retorna uso de CPU (%)
     */
    float getCPUUsage();

    /**
     * @brief Verifica se sistema está saudável
     */
    bool isHealthy();

    /**
     * @brief Retorna status dos subsistemas (bitmask)
     */
    uint8_t getSystemStatus();

    /**
     * @brief Define início da missão
     */
    void startMission();

    /**
     * @brief Verifica se missão está ativa
     */
    bool isMissionActive();

    /**
     * @brief Retorna contador de erros
     */
    uint16_t getErrorCount();

    /**
     * @brief Reseta contador de erros
     */
    void resetErrorCount();

    /**
     * @brief Retorna estatísticas do sistema
     */
    void getStatistics(uint32_t& uptime, uint32_t& freeHeap,
                       float& cpuTemp, uint16_t& errors);

    /**
     * @brief Retorna status atual de heap
     */
    HeapStatus getHeapStatus() const { return _heapStatus; }

private:
    // Monitoramento geral
    uint32_t _lastHealthCheck = 0;

    // Monitoramento de heap
    uint32_t _lastHeapCheck   = 0;
    uint32_t _minHeapSeen     = UINT32_MAX;
    HeapStatus _heapStatus    = HeapStatus::OK;

    bool _healthy             = true;
    uint8_t _systemStatus     = 0;
    uint16_t _errorCount      = 0;

    unsigned long _bootTime        = 0;
    unsigned long _missionStartTime = 0;
    bool _missionActive            = false;

    uint32_t _lastWatchdogFeed = 0;

    // Estatísticas adicionais
    uint32_t _minFreeHeap = 0;
    float _maxCPUTemp     = 0.0f;

    /**
     * @brief Verifica saúde geral do sistema
     */
    void _checkSystemHealth();

    /**
     * @brief Atualiza estado relacionado a heap (baixa memória)
     */
    void _updateHeapStatus(uint32_t now);

    /**
     * @brief Lê temperatura interna do ESP32
     */
    float _readCPUTemperature();
};

#endif // SYSTEM_HEALTH_H
