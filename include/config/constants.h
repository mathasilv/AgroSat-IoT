/**
 * @file constants.h
 * @brief Constantes, limites e parâmetros de configuração do sistema
 * 
 * @details Arquivo central com todas as constantes configuráveis:
 *          - Parâmetros de comunicação (LoRa, WiFi, HTTP)
 *          - Limites de validação de sensores
 *          - Timeouts e intervalos
 *          - Configurações de energia
 *          - Paths de arquivos SD
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Ajustes Comuns
 * | Parâmetro            | Padrão    | Descrição                    |
 * |----------------------|-----------|------------------------------|
 * | TEAM_ID              | 666       | ID da equipe na competição   |
 * | LORA_TX_POWER        | 20 dBm    | Potência de transmissão      |
 * | LORA_SPREADING_FACTOR| 7         | SF para modo normal          |
 * | WIFI_TIMEOUT_MS      | 10000     | Timeout de conexão WiFi      |
 * | BATTERY_LOW          | 3.7V      | Limiar de bateria baixa      |
 * 
 * @note Valores de validação baseados nos datasheets dos sensores
 * @warning Alterar LORA_TX_POWER acima de 20dBm pode violar regulamentação
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

//=============================================================================
// IDENTIFICAÇÃO
//=============================================================================
#define TEAM_ID 666             ///< ID da equipe (usado no payload HTTP)

//=============================================================================
// I2C
//=============================================================================
#define I2C_FREQUENCY 100000    ///< Frequência do barramento (100kHz standard)
#define I2C_TIMEOUT_MS 3000     ///< Timeout de operações I2C     

//=============================================================================
// POWER MANAGEMENT
//=============================================================================
#define BATTERY_SAMPLES 16      ///< Amostras ADC para média
#define BATTERY_VREF 3.6        ///< Tensão de referência ADC (V)
#define BATTERY_DIVIDER 2.0     ///< Fator do divisor de tensão
#define BATTERY_LOW 3.7         ///< Limiar bateria baixa (V)
#define BATTERY_CRITICAL 3.3    ///< Limiar bateria crítica (V)

//=============================================================================
// TIMING (ms)
//=============================================================================
#define BUTTON_DEBOUNCE_TIME 50         ///< Debounce do botão
#define BUTTON_LONG_PRESS_TIME 3000     ///< Tempo para long press
#define WIFI_TIMEOUT_MS 10000           ///< Timeout conexão WiFi
#define HTTP_TIMEOUT_MS 5000            ///< Timeout requisição HTTP
#define SYSTEM_HEALTH_INTERVAL 10000    ///< Intervalo verificação saúde

//=============================================================================
// WATCHDOG (segundos)
//=============================================================================
#define WATCHDOG_TIMEOUT_PREFLIGHT 60   ///< WDT em modo PREFLIGHT
#define WATCHDOG_TIMEOUT_FLIGHT 90      ///< WDT em modo FLIGHT
#define WATCHDOG_TIMEOUT_SAFE 180       ///< WDT em modo SAFE       

//=============================================================================
// LORA SX1276
//=============================================================================
#define LORA_FREQUENCY 915E6            ///< Frequência central (Hz)
#define LORA_SPREADING_FACTOR 7         ///< SF modo normal (7-12)
#define LORA_SPREADING_FACTOR_SAFE 12   ///< SF modo SAFE (máximo alcance)
#define LORA_SIGNAL_BANDWIDTH 125E3     ///< Largura de banda (Hz)
#define LORA_CODING_RATE 5              ///< Coding rate (4/5)
#define LORA_TX_POWER 20                ///< Potência TX (dBm, max 20)
#define LORA_PREAMBLE_LENGTH 8          ///< Símbolos de preâmbulo
#define LORA_SYNC_WORD 0x12             ///< Palavra de sincronização
#define LORA_CRC_ENABLED true           ///< Habilitar CRC
#define LORA_LDRO_ENABLED true          ///< Low Data Rate Optimization
#define LORA_MAX_TX_TIME_MS 400         ///< Tempo máximo por TX
#define LORA_DUTY_CYCLE_PERCENT 10      ///< Duty cycle máximo (%)
#define LORA_DUTY_CYCLE_WINDOW_MS 3600000 ///< Janela de duty cycle (1h) 

//=============================================================================
// WIFI & HTTP
//=============================================================================
#define WIFI_SSID "MATHEUS "            ///< SSID da rede WiFi
#define WIFI_PASSWORD "12213490"        ///< Senha da rede WiFi
#define HTTP_SERVER "obsat.org.br"      ///< Servidor HTTP
#define HTTP_PORT 443                   ///< Porta (443 = HTTPS)
#define HTTP_ENDPOINT "/teste_post/envio.php" ///< Endpoint da API

//=============================================================================
// BUFFERS E LIMITES
//=============================================================================
#define JSON_MAX_SIZE 2048              ///< Tamanho máximo JSON (bytes)
#define PAYLOAD_MAX_SIZE 64             ///< Tamanho máximo payload
#define MAX_GROUND_NODES 3              ///< Máximo de ground nodes
#define NODE_TTL_MS 1800000             ///< TTL de nó (30 min)             

//=============================================================================
// SD CARD - ARQUIVOS
//=============================================================================
#define SD_LOG_FILE "/telemetry.csv"    ///< Arquivo de telemetria
#define SD_MISSION_FILE "/mission.csv"  ///< Arquivo de missão
#define SD_ERROR_FILE "/errors.log"     ///< Log de erros
#define SD_SYSTEM_LOG "/system.log"     ///< Log do sistema
#define SD_MAX_FILE_SIZE 5242880        ///< Tamanho máximo (5MB)

//=============================================================================
// MISSÃO
//=============================================================================
#define MISSION_DURATION_MS 7200000     ///< Duração máxima (2 horas) 

//=============================================================================
// RTC E NTP
//=============================================================================
#define NTP_SERVER_PRIMARY   "pool.ntp.org"   ///< Servidor NTP primário
#define NTP_SERVER_SECONDARY "time.nist.gov"  ///< Servidor NTP secundário
#define RTC_TIMEZONE_OFFSET (-3 * 3600)       ///< Fuso horário (UTC-3)

//=============================================================================
// DEBUG
//=============================================================================
#define DEBUG_BAUDRATE 115200           ///< Baudrate da Serial de debug

//=============================================================================
// LIMITES DE VALIDAÇÃO DE SENSORES
//=============================================================================

// Temperatura (°C)
#define TEMP_MIN_VALID -90.0f           ///< Mínimo físico
#define TEMP_MAX_VALID 100.0f           ///< Máximo físico

// Pressão (hPa)
#define PRESSURE_MIN_VALID 5.0f         ///< Mínimo (vácuo parcial)
#define PRESSURE_MAX_VALID 1100.0f      ///< Máximo (nível do mar)

// Umidade (%RH)
#define HUMIDITY_MIN_VALID 0.0f         ///< Mínimo
#define HUMIDITY_MAX_VALID 100.0f       ///< Máximo

// CO2 (ppm) - CCS811
#define CO2_MIN_VALID 350.0f            ///< Mínimo atmosférico
#define CO2_MAX_VALID 8192.0f           ///< Máximo do sensor

// TVOC (ppb) - CCS811
#define TVOC_MIN_VALID 0.0f             ///< Mínimo
#define TVOC_MAX_VALID 1200.0f          ///< Máximo do sensor

// Magnetômetro (µT) - AK8963
#define MAG_MIN_VALID -4800.0f          ///< Mínimo do sensor
#define MAG_MAX_VALID 4800.0f           ///< Máximo do sensor

// Acelerômetro (g) - MPU9250
#define ACCEL_MIN_VALID -16.0f          ///< Mínimo (±16g range)
#define ACCEL_MAX_VALID 16.0f           ///< Máximo (±16g range)

// Giroscópio (°/s) - MPU9250
#define GYRO_MIN_VALID -2000.0f         ///< Mínimo (±2000°/s range)
#define GYRO_MAX_VALID 2000.0f          ///< Máximo (±2000°/s range)

#endif // CONSTANTS_H