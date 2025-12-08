/**
 * @file main.cpp
 * @brief Programa principal - Integração Completa (LoRa, Sensores, GPS)
 * @version 9.0.0 - I2C Clock Corrigido (100kHz)
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
    // 🚑 CONFIGURAÇÃO I2C CORRIGIDA (CRÍTICO 3)
    // ============================================================
    DEBUG_PRINTLN("[Main] Configurando I2C Mestre...");
    
    Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    
    // CORRIGIDO: 100kHz Standard Mode (de 50kHz)
    Wire.setClock(I2C_FREQUENCY);  // 100000 Hz
    Wire.setTimeout(I2C_TIMEOUT_MS);
    Wire.setBufferSize(512);  // ADICIONADO: Buffer maior para estabilidade
    
    DEBUG_PRINTF("[Main] I2C Configurado: %d kHz, Timeout %d ms\n", 
                 I2C_FREQUENCY/1000, I2C_TIMEOUT_MS);
    delay(500); 
    // ============================================================

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO AGROSAT-IOT v9.0 (FIXED)");
    DEBUG_PRINTLN("[Main] ========================================");
    
    // Watchdog com timeout adaptativo
    esp_task_wdt_init(WATCHDOG_TIMEOUT_PREFLIGHT, true);
    esp_task_wdt_add(NULL);
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização de subsistemas!");
    } else {
        DEBUG_PRINTLN("[Main] Inicialização completa com sucesso.");
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
    DEBUG_PRINTLN("=== COMANDOS DISPONÍVEIS ===");
    DEBUG_PRINTLN("  STATUS          : Status detalhado dos sensores");
    DEBUG_PRINTLN("  CALIB_MAG       : Calibra magnetômetro (hard+soft iron)");
    DEBUG_PRINTLN("  CLEAR_MAG       : Apaga calibração do magnetômetro");
    DEBUG_PRINTLN("  SAVE_BASELINE   : Salva baseline CCS811");
    DEBUG_PRINTLN("  START_MISSION   : Inicia modo FLIGHT");
    DEBUG_PRINTLN("  STOP_MISSION    : Retorna ao modo PREFLIGHT");
    DEBUG_PRINTLN("  SAFE_MODE       : Força modo SAFE");
    DEBUG_PRINTLN("  LINK_BUDGET     : Mostra cálculo de link budget");
    DEBUG_PRINTLN("  HELP            : Este menu");
    DEBUG_PRINTLN("============================");
}