/**
 * @file main.cpp
 * @brief Ponto de entrada principal do sistema AgroSat-IoT
 * 
 * @details Sistema de telemetria agrícola baseado em ESP32 com FreeRTOS.
 *          Gerencia sensores ambientais, comunicação LoRa e armazenamento
 *          de dados para monitoramento de culturas e condições climáticas.
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 10.9.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * @note Requer ESP32 com FreeRTOS habilitado
 * @warning Watchdog configurado - alimentar regularmente
 * 
 * ## Arquitetura de Tasks
 * | Task         | Core | Prioridade | Stack | Função                    |
 * |--------------|------|------------|-------|---------------------------|
 * | SensorsTask  | 1    | 2          | 4KB   | Leitura de sensores 10Hz  |
 * | HttpTask     | 0    | 1          | 8KB   | Processamento HTTP        |
 * | StorageTask  | 0    | 1          | 8KB   | Persistência em SD Card   |
 * 
 * ## Changelog
 * - v10.9.0: Verificação de criação de tasks com restart automático
 * - v10.8.0: Separação de tasks por núcleo
 * - v10.0.0: Migração para arquitetura multi-task
 */
#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Globals.h" 
#include "config.h"
#include "app/TelemetryManager/TelemetryManager.h"

TelemetryManager telemetry;

//=============================================================================
// FORWARD DECLARATIONS
//=============================================================================

void processSerialCommands();                ///< Processa comandos via Serial
void printAvailableCommands();               ///< Exibe menu de comandos
void vTaskHttp(void *pvParameters);          ///< Task de processamento HTTP
void vTaskStorage(void *pvParameters);       ///< Task de armazenamento SD
void vTaskSensors(void *pvParameters);       ///< Task de leitura de sensores

//=============================================================================
// TASK HANDLES
//=============================================================================

TaskHandle_t hTaskHttp = NULL;               ///< Handle da task HTTP
TaskHandle_t hTaskStorage = NULL;            ///< Handle da task Storage
TaskHandle_t hTaskSensors = NULL;            ///< Handle da task Sensores

//=============================================================================
// SETUP - INICIALIZAÇÃO DO SISTEMA
//=============================================================================

/**
 * @brief Inicialização do sistema
 * 
 * @details Sequência de inicialização:
 *          1. Recursos globais (mutexes e filas FreeRTOS)
 *          2. Barramento I2C com proteção de mutex
 *          3. Periféricos (LED, botão)
 *          4. Watchdog timer
 *          5. Subsistema de telemetria
 *          6. Criação das tasks FreeRTOS
 * 
 * @note Em caso de falha na criação de tasks, o sistema reinicia
 */
void setup() {
    // 1. Recursos Globais (Mutexes e Filas)
    initGlobalResources();

    Serial.begin(DEBUG_BAUDRATE);
    
    // 2. I2C Seguro
    DEBUG_PRINTLN("[Main] Configurando I2C Mestre...");
    if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdTRUE) {
        Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
        Wire.setClock(I2C_FREQUENCY);
        Wire.setTimeout(I2C_TIMEOUT_MS);
        Wire.setBufferSize(512); 
        xSemaphoreGive(xI2CMutex);
        DEBUG_PRINTF("[Main] I2C Configurado: %d kHz\n", I2C_FREQUENCY/1000);
    }
    delay(500); 

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] AGROSAT-IOT v10.9 (Task Verification)");
    DEBUG_PRINTLN("[Main] ========================================");
    
    esp_task_wdt_init(WATCHDOG_TIMEOUT_PREFLIGHT, true);
    esp_task_wdt_add(NULL);
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRITICO: Falha na inicializacao!");
    } else {
        DEBUG_PRINTLN("[Main] Inicializacao completa.");
    }

    // 3. TAREFAS - FIX: Verificação de criação
    BaseType_t taskResult;
    
    // Tarefa de Sensores (Alta Prioridade - Tempo Real)
    taskResult = xTaskCreatePinnedToCore(
        vTaskSensors,    "SensorsTask",   4096, NULL, 
        2, &hTaskSensors, 1
    );
    if (taskResult != pdPASS) {
        DEBUG_PRINTLN("[Main] ERRO CRITICO: Falha ao criar SensorsTask!");
        delay(1000);
        ESP.restart();
    }
    DEBUG_PRINTLN("[Main] SensorsTask criada com sucesso.");

    // Tarefa HTTP
    taskResult = xTaskCreatePinnedToCore(
        vTaskHttp, "HttpTask", 8192, NULL, 1, &hTaskHttp, 0
    );
    if (taskResult != pdPASS) {
        DEBUG_PRINTLN("[Main] ERRO CRITICO: Falha ao criar HttpTask!");
        delay(1000);
        ESP.restart();
    }
    DEBUG_PRINTLN("[Main] HttpTask criada com sucesso.");

    // Tarefa Storage (8KB stack para JSON + SD Card)
    taskResult = xTaskCreatePinnedToCore(
        vTaskStorage, "StorageTask", 8192, NULL, 1, &hTaskStorage, 0
    );
    if (taskResult != pdPASS) {
        DEBUG_PRINTLN("[Main] ERRO CRITICO: Falha ao criar StorageTask!");
        delay(1000);
        ESP.restart();
    }
    DEBUG_PRINTLN("[Main] StorageTask criada com sucesso.");
    
    DEBUG_PRINTF("[Main] Heap livre apos tasks: %lu bytes\n", ESP.getFreeHeap());
    
    printAvailableCommands();
}

//=============================================================================
// LOOP PRINCIPAL
//=============================================================================

/**
 * @brief Loop principal (baixa prioridade)
 * 
 * @details Executa tarefas não críticas em tempo:
 *          - Alimentação do watchdog
 *          - Processamento de comandos serial
 *          - Gerenciamento de comunicação rádio LoRa
 * 
 * @note Tarefas críticas (sensores, storage) rodam em tasks dedicadas
 */
void loop() {
    telemetry.feedWatchdog(); 
    processSerialCommands();
    
    // Loop leve: apenas gerencia envio e recebimento de rádio
    telemetry.loop();
    
    delay(10); 
}

//=============================================================================
// TASKS FREERTOS
//=============================================================================

/**
 * @brief Task de leitura de sensores (tempo real)
 * 
 * @param pvParameters Parâmetros da task (não utilizado)
 * 
 * @details Executa leitura dos sensores físicos a 10Hz (100ms)
 *          usando vTaskDelayUntil() para timing preciso.
 *          Sensores: MPU9250, BMP280, SI7021, CCS811, GPS
 * 
 * @note Pinned ao Core 1 para isolamento de tempo real
 * @warning Não bloquear esta task por mais de 100ms
 */
void vTaskSensors(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms = 10Hz
    
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        telemetry.updatePhySensors();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Task de processamento HTTP
 * 
 * @param pvParameters Parâmetros da task (não utilizado)
 * 
 * @details Aguarda mensagens na fila xHttpQueue e processa
 *          requisições HTTP de forma assíncrona.
 *          Usado para envio de dados para servidor remoto.
 * 
 * @note Bloqueante - aguarda indefinidamente por mensagens
 */
void vTaskHttp(void *pvParameters) {
    HttpQueueMessage msg;
    for (;;) {
        if (xQueueReceive(xHttpQueue, &msg, portMAX_DELAY) == pdTRUE) {
            telemetry.processHttpPacket(msg);
        }
    }
}

/**
 * @brief Task de armazenamento em SD Card
 * 
 * @param pvParameters Parâmetros da task (não utilizado)
 * 
 * @details Aguarda sinais na fila xStorageQueue para persistir
 *          dados de telemetria no cartão SD em formato CSV.
 * 
 * @note Stack de 8KB para suportar buffers JSON + operações SD
 */
void vTaskStorage(void *pvParameters) {
    uint8_t signal;
    StorageQueueMessage dummyMsg; // Apenas para manter compatibilidade da API
    for (;;) {
        if (xQueueReceive(xStorageQueue, &signal, portMAX_DELAY) == pdTRUE) {
            telemetry.processStoragePacket(dummyMsg);
        }
    }
}

//=============================================================================
// FUNÇÕES AUXILIARES
//=============================================================================

/**
 * @brief Processa comandos recebidos via interface Serial
 * 
 * @details Lê comandos da Serial, converte para maiúsculas e
 *          delega para o CommandHandler do TelemetryManager.
 *          Comandos suportados: STATUS, START_MISSION, STOP_MISSION, etc.
 * 
 * @note Comandos são case-insensitive
 * @see printAvailableCommands() para lista completa
 */
void processSerialCommands() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd.length() == 0) return;
    DEBUG_PRINTF("[Main] Comando recebido: %s\n", cmd.c_str());
    if (!telemetry.handleCommand(cmd)) {
        DEBUG_PRINTLN("[Main] Comando nao reconhecido (use HELP)");
    }
}

/**
 * @brief Exibe lista de comandos disponíveis no console Serial
 */
void printAvailableCommands() {
    DEBUG_PRINTLN("=== COMANDOS DISPONIVEIS ===");
    DEBUG_PRINTLN("  STATUS          : Status detalhado");
    DEBUG_PRINTLN("  START_MISSION   : Inicia modo FLIGHT");
    DEBUG_PRINTLN("  STOP_MISSION    : Retorna ao modo PREFLIGHT");
    DEBUG_PRINTLN("  SAFE_MODE       : Forca modo SAFE");
    DEBUG_PRINTLN("  MUTEX_STATS     : Estatisticas de mutex");
    DEBUG_PRINTLN("  HELP            : Este menu");
    DEBUG_PRINTLN("============================");
}
