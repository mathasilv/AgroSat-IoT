/**
 * @file main.cpp
 * @brief Programa principal - Sistema Simplificado com CORREÇÃO I2C
 * @version 6.1.1
 * @date 2025-12-02
 */
#include <Arduino.h>
#include <Wire.h> // <--- GARANTA QUE ISTO ESTÁ AQUI
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
    // 🚑 CONFIGURAÇÃO DE SEGURANÇA I2C (ATUALIZADO)
    // ============================================================
    DEBUG_PRINTLN("[Main] Configurando I2C Mestre...");
    
    Wire.begin(SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    
    // Volte para o padrão 100kHz (mais estável que 20kHz para o timer do ESP32)
    Wire.setClock(100000); 
    
    // Timeout ALTO (1000ms) - O CCS811 precisa disso!
    Wire.setTimeOut(1000); 
    
    DEBUG_PRINTLN("[Main] I2C Configurado: 100kHz, Timeout 1000ms");
    delay(500); 
    // ============================================================

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
    
    // Agora o telemetry.begin() vai usar o barramento que JÁ configuramos acima
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] SISTEMA OPERACIONAL");
    DEBUG_PRINTLN("[Main] Modo: PRE-FLIGHT");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    
    // Exibir comandos disponíveis
    printAvailableCommands();
}

void loop() {
    esp_task_wdt_reset();
    
    // Processar comandos via Serial (se disponível)
    processSerialCommands();
    
    // Loop principal
    telemetry.loop();
    
    // Pequeno delay para aliviar o I2C e permitir WiFi/LoRa background tasks
    delay(10); 
}

/**
 * @brief Processa comandos via Serial Monitor
 */
void processSerialCommands() {
    if (!Serial.available()) {
        return;
    }
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd.length() == 0) {
        return;
    }
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("[Main] Comando recebido: %s\n", cmd.c_str());
    DEBUG_PRINTLN("========================================");
    
    // Delegar comandos para TelemetryManager
    if (!telemetry.handleCommand(cmd)) {
        DEBUG_PRINTLN("[Main] ⚠ Comando não reconhecido");
        DEBUG_PRINTLN("[Main] Digite HELP para ver comandos disponíveis");
    }
    
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
}

/**
 * @brief Exibe comandos disponíveis no boot
 */
void printAvailableCommands() {
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("COMANDOS DISPONÍVEIS VIA SERIAL:");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("SENSORES:");
    DEBUG_PRINTLN("  STATUS_SENSORES   - Status detalhado");
    DEBUG_PRINTLN("  RECALIBRAR_MAG    - Recalibrar magnetômetro");
    DEBUG_PRINTLN("  LIMPAR_MAG        - Limpar calibração salva");
    DEBUG_PRINTLN("  VER_MAG           - Ver calibração atual");
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("CCS811:");
    DEBUG_PRINTLN("  SALVAR_BASELINE   - Salvar baseline (após 48h)");
    DEBUG_PRINTLN("  RESTAURAR_BASELINE- Restaurar baseline salvo");
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("SISTEMA:");
    DEBUG_PRINTLN("  HELP              - Mostrar este menu");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
}