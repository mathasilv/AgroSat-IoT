/**
 * @file main.cpp
 * @brief Programa principal - Sistema Simplificado
 * @version 6.0.0
 * @date 2025-11-24
 */
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "TelemetryManager.h"

TelemetryManager telemetry;

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    delay(1000);
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO SISTEMA DE TELEMETRIA");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTF("[Main] Botão de controle: GPIO %d\n", BUTTON_PIN);
    DEBUG_PRINTLN("[Main]   - Pressionar: Toggle PREFLIGHT ↔ FLIGHT");
    DEBUG_PRINTLN("[Main]   - Segurar 3s: Ativar SAFE MODE");
    DEBUG_PRINTLN("");
    
    DEBUG_PRINTLN("[Main] Configurando Watchdog Timer (120s)...");
    esp_task_wdt_init(120, true);
    esp_task_wdt_add(NULL);
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] SISTEMA OPERACIONAL");
    DEBUG_PRINTLN("[Main] Modo: PRE-FLIGHT");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
}

void loop() {
    esp_task_wdt_reset();
    
    telemetry.loop();
    
    delay(10);
}
