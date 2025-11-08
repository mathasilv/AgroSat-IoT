/**
 * @file mission_config.h
 * @brief Parametrização dos timeouts, intervalos e limites críticos para todos subsistemas do CubeSat
 * @version 1.0.0
 * @date 2025-11-07
 *
 * Todos os valores centralizados aqui permitem:
 * - Facilidade de ajuste para missões diferentes
 * - Permite testes unitários nos limites
 *
 * Watchdog: Tempo máximo sem resposta para realizar reset
 * Heap: Limite mínimo de memória antes de entrar em modo de erro
 * Timeout de inicialização: sensores e periféricos
 */

#ifndef MISSION_CONFIG_H
#define MISSION_CONFIG_H

// Watchdog timeout (em segundos)
#define WATCHDOG_TIMEOUT_S        60
#define WATCHDOG_RESET_HEAP_BYTES 10000   // Min heap crítico para reset

// Intervalos [ms]
#define SYSTEM_HEALTH_INTERVAL_MS 10000   // Rotina de health monitor
#define TELEMETRY_SEND_INTERVAL_MS 30000
#define STORAGE_SAVE_INTERVAL_MS   60000
#define DISPLAY_UPDATE_INTERVAL_MS 2000

// Timeout inicialização sensores [ms]
#define SENSOR_INIT_TIMEOUT_MS    2000
#define CCS811_WARMUP_TIME_MS     20000

// Limites para sensores
#define MAX_ALTITUDE_M            30000
#define MIN_TEMPERATURE_C         -80
#define MAX_TEMPERATURE_C         85
#define TEMP_MIN_VALID_C          -50.0
#define TEMP_MAX_VALID_C          100.0
#define PRESSURE_MIN_VALID_HPA    300.0
#define PRESSURE_MAX_VALID_HPA    1100.0
#define HUMIDITY_MIN_VALID_PC     0.0
#define HUMIDITY_MAX_VALID_PC     100.0
#define CO2_MIN_VALID_PPM         350.0
#define CO2_MAX_VALID_PPM         5000.0
#define TVOC_MIN_VALID_PPB        0.0
#define TVOC_MAX_VALID_PPB        1000.0

#endif // MISSION_CONFIG_H
