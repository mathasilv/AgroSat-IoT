/**
 * @file main.cpp
 * @brief Programa principal - Integração Completa (LoRa, Sensores, GPS)
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
    // 🚑 CONFIGURAÇÃO DE SEGURANÇA I2C
    // ============================================================
    DEBUG_PRINTLN("[Main] Configurando I2C Mestre...");
    
    // Inicia o barramento I2C
    Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    
    // Configurações para estabilidade com cabos longos e sensores lentos (CCS811)
    Wire.setClock(50000);   // 50kHz
    Wire.setTimeout(3000);  // 3000ms
    
    DEBUG_PRINTLN("[Main] I2C Configurado: 50kHz, Timeout 3000ms");
    delay(500); 
    // ============================================================

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO AGROSAT-IOT v7.2 (GPS)");
    DEBUG_PRINTLN("[Main] ========================================");
    
    // Watchdog
    esp_task_wdt_init(120, true);
    esp_task_wdt_add(NULL);
    
    // Inicialização de todos os gerenciadores (incluindo GPS)
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
    
    // Loop principal (processa GPS, LoRa, Sensores)
    telemetry.loop();
    
    // Pequeno delay para estabilidade do loop sem travar o RTOS
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