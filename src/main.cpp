/**
 * @file main.cpp
 * @brief Programa principal - CORREÇÃO DE TIMEOUT I2C
 */
#include <Arduino.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "app/TelemetryManager/TelemetryManager.h"

TelemetryManager telemetry;

// Forward declarations
void processSerialCommands();
void printAvailableCommands();

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    
    // ============================================================
    // 🚑 CONFIGURAÇÃO DE SEGURANÇA I2C (CORRIGIDA)
    // ============================================================
    DEBUG_PRINTLN("[Main] Configurando I2C Mestre...");
    
    // Inicia o barramento
    Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    
    // CONFIGURAÇÃO CRÍTICA PARA ERRO 263:
    // 1. Clock lento (50kHz) para tolerar cabos longos/ruído
    Wire.setClock(50000); 
    
    // 2. Timeout MUITO ALTO (3000ms). O CCS811 pode segurar o clock (stretching)
    // por tempo indeterminado se estiver ocupado. O padrão do ESP32 é curto.
    Wire.setTimeout(3000);  
    
    DEBUG_PRINTLN("[Main] I2C Configurado: 50kHz, Timeout 3000ms (FIX 263)");
    delay(500); 
    // ============================================================

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO SISTEMA DE TELEMETRIA");
    DEBUG_PRINTLN("[Main] ========================================");
    
    // Watchdog
    esp_task_wdt_init(120, true);
    esp_task_wdt_add(NULL);
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
    }
    
    printAvailableCommands();
}

void loop() {
    esp_task_wdt_reset();
    processSerialCommands();
    telemetry.loop();
    delay(10); 
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
    DEBUG_PRINTLN("--- COMANDOS: STATUS_SENSORES, RECALIBRAR_MAG, SALVAR_BASELINE, HELP ---");
}