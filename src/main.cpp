/**
 * @file main.cpp
 * @brief Programa principal - Fase 4 (High Precision Sensors)
 * @version 10.8.0
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

// Forward declarations
void processSerialCommands();
void printAvailableCommands();
void vTaskHttp(void *pvParameters); 
void vTaskStorage(void *pvParameters); 
void vTaskSensors(void *pvParameters); // NOVO

// Handles
TaskHandle_t hTaskHttp = NULL;
TaskHandle_t hTaskStorage = NULL; 
TaskHandle_t hTaskSensors = NULL; // NOVO

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
    DEBUG_PRINTLN("[Main] AGROSAT-IOT v10.8 (Precision Sensors)");
    DEBUG_PRINTLN("[Main] ========================================");
    
    esp_task_wdt_init(WATCHDOG_TIMEOUT_PREFLIGHT, true);
    esp_task_wdt_add(NULL);
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
    } else {
        DEBUG_PRINTLN("[Main] Inicialização completa.");
    }

    // 3. TAREFAS
    
    // Tarefa de Sensores (Alta Prioridade - Tempo Real)
    // Roda no Core 1 (App Core) para máxima performance de cálculo
    xTaskCreatePinnedToCore(
        vTaskSensors,    "SensorsTask",   4096, NULL, 
        2, // Prioridade 2 (Maior que HTTP/Storage/Loop)
        &hTaskSensors,   1
    );

    // Tarefas de I/O (Baixa Prioridade) - Core 0
    xTaskCreatePinnedToCore(vTaskHttp,    "HttpTask",    8192, NULL, 1, &hTaskHttp,    0);
    xTaskCreatePinnedToCore(vTaskStorage, "StorageTask", 4096, NULL, 1, &hTaskStorage, 0);
    
    printAvailableCommands();
}

void loop() {
    telemetry.feedWatchdog(); 
    processSerialCommands();
    
    // Loop agora é leve: apenas gerencia envio e recebimento de rádio
    telemetry.loop();
    
    delay(10); 
}

// === TAREFA SENSORES (NOVO FASE 4) ===
// Executa a cada 100ms com precisão, independente do resto do sistema.
void vTaskSensors(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms = 10Hz
    
    // Inicializa o tempo de referência
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Atualiza Sensores, GPS e Power (protegido por Mutex)
        telemetry.updatePhySensors();
        
        // Espera até o próximo ciclo exato
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// === TAREFA HTTP ===
void vTaskHttp(void *pvParameters) {
    HttpQueueMessage msg;
    for (;;) {
        if (xQueueReceive(xHttpQueue, &msg, portMAX_DELAY) == pdTRUE) {
            telemetry.processHttpPacket(msg);
        }
    }
}

// === TAREFA STORAGE ===
void vTaskStorage(void *pvParameters) {
    StorageQueueMessage msg;
    for (;;) {
        if (xQueueReceive(xStorageQueue, &msg, portMAX_DELAY) == pdTRUE) {
            telemetry.processStoragePacket(msg);
        }
    }
}

void processSerialCommands() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd.length() == 0) return;
    DEBUG_PRINTF("[Main] Comando recebido: %s\n", cmd.c_str());
    if (!telemetry.handleCommand(cmd)) {
        DEBUG_PRINTLN("[Main] Comando não reconhecido (use HELP)");
    }
}

void printAvailableCommands() {
    DEBUG_PRINTLN("=== COMANDOS DISPONÍVEIS ===");
    DEBUG_PRINTLN("  STATUS          : Status detalhado");
    DEBUG_PRINTLN("  START_MISSION   : Inicia modo FLIGHT");
    DEBUG_PRINTLN("  STOP_MISSION    : Retorna ao modo PREFLIGHT");
    DEBUG_PRINTLN("  SAFE_MODE       : Força modo SAFE");
    DEBUG_PRINTLN("  HELP            : Este menu");
    DEBUG_PRINTLN("============================");
}