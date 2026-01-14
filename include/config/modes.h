/**
 * @file modes.h
 * @brief Definições de modos de operação e suas configurações
 * 
 * @details Define os estados operacionais do sistema e as configurações
 *          específicas de cada modo:
 *          - PREFLIGHT: Inicialização e testes
 *          - FLIGHT: Coleta ativa de dados
 *          - SAFE: Modo de emergência/economia
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Comparação de Modos
 * | Parâmetro          | PREFLIGHT | FLIGHT  | SAFE     |
 * |--------------------|-----------|---------|----------|
 * | Serial Logs        | ✓         | ✗       | ✓        |
 * | SD Verbose         | ✓         | ✗       | ✓        |
 * | LoRa               | ✓         | ✓       | ✓        |
 * | HTTP               | ✓         | ✓       | ✗        |
 * | Telemetry Interval | 20s       | 60s     | 120s     |
 * | Storage Interval   | 1s        | 10s     | 300s     |
 * | Beacon             | -         | -       | 180s     |
 * 
 * ## Diagrama de Estados
 * ```
 *                    ┌─────────────┐
 *         ┌─────────►│  PREFLIGHT  │◄─────────┐
 *         │          └──────┬──────┘          │
 *         │                 │ START_MISSION   │
 *         │                 ▼                 │
 *         │          ┌─────────────┐          │
 *         │          │   FLIGHT    │──────────┤
 *         │          └──────┬──────┘          │
 *         │                 │ STOP/ERROR      │
 *         │                 ▼                 │
 *         │          ┌─────────────┐          │
 *         └──────────│    SAFE     │──────────┘
 *                    └─────────────┘
 * ```
 * 
 * @see TelemetryManager::applyModeConfig() para aplicação
 */

#ifndef MODES_H
#define MODES_H

#include <stdint.h>

//=============================================================================
// MODOS DE OPERAÇÃO
//=============================================================================

/**
 * @enum OperationMode
 * @brief Estados operacionais do sistema
 */
enum OperationMode : uint8_t {
    MODE_INIT = 0,       ///< Inicialização (transitório)
    MODE_PREFLIGHT = 1,  ///< Pré-voo: testes e configuração
    MODE_FLIGHT = 2,     ///< Voo: coleta ativa de dados
    MODE_POSTFLIGHT = 3, ///< Pós-voo: análise (não implementado)
    MODE_SAFE = 4,       ///< Seguro: emergência/economia
    MODE_ERROR = 5       ///< Erro: falha crítica
};

//=============================================================================
// ESTRUTURA DE CONFIGURAÇÃO DE MODO
//=============================================================================

/**
 * @struct ModeConfig
 * @brief Configurações específicas para cada modo de operação
 */
struct ModeConfig {
    bool displayEnabled;           ///< Habilitar display (se houver)
    bool serialLogsEnabled;        ///< Habilitar logs na Serial
    bool sdLogsVerbose;            ///< Logs detalhados no SD
    bool loraEnabled;              ///< Habilitar transmissão LoRa
    bool httpEnabled;              ///< Habilitar envio HTTP
    uint32_t telemetrySendInterval;///< Intervalo de envio LoRa (ms)
    uint32_t storageSaveInterval;  ///< Intervalo de gravação SD (ms)
    uint32_t beaconInterval;       ///< Intervalo de beacon (ms, 0=desabilitado)
};

//=============================================================================
// CONFIGURAÇÕES PRÉ-DEFINIDAS
//=============================================================================

/**
 * @brief Configuração do modo PREFLIGHT
 * @details Modo de inicialização com todos os logs habilitados
 *          para diagnóstico e verificação do sistema.
 */
const ModeConfig PREFLIGHT_CONFIG = {
    .displayEnabled = true,
    .serialLogsEnabled = true,        // Logs completos para debug
    .sdLogsVerbose = true,
    .loraEnabled = true,
    .httpEnabled = true,
    .telemetrySendInterval = 20000,   // 20s - frequente para testes
    .storageSaveInterval = 1000,      // 1s - máxima resolução
    .beaconInterval = 0               // Sem beacon
};

/**
 * @brief Configuração do modo FLIGHT
 * @details Modo de operação normal com logs reduzidos
 *          para economizar energia e banda.
 */
const ModeConfig FLIGHT_CONFIG = {
    .displayEnabled = false,
    .serialLogsEnabled = false,       // Logs desabilitados
    .sdLogsVerbose = false,
    .loraEnabled = true,
    .httpEnabled = true,
    .telemetrySendInterval = 60000,   // 60s - economia de duty cycle
    .storageSaveInterval = 10000,     // 10s - balanço resolução/espaço
    .beaconInterval = 0               // Sem beacon
};

/**
 * @brief Configuração do modo SAFE
 * @details Modo de emergência/economia com mínimo consumo.
 *          Envia beacons periódicos para localização.
 */
const ModeConfig SAFE_CONFIG = {
    .displayEnabled = false,
    .serialLogsEnabled = true,        // Logs para diagnóstico
    .sdLogsVerbose = true,
    .loraEnabled = true,
    .httpEnabled = false,             // HTTP desabilitado (economia)
    .telemetrySendInterval = 120000,  // 2min - máxima economia
    .storageSaveInterval = 300000,    // 5min - mínimo uso de SD
    .beaconInterval = 180000          // 3min - beacon de localização
};

#endif // MODES_H