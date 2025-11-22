/**
 * @file main.cpp
 * @brief Programa principal com dual-mode + SAFE MODE + watchdog fix
 * @version 4.0.0
 * @date 2025-11-11
 */
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "TelemetryManager.h"

TelemetryManager telemetry;
bool missionStarted = false;
bool bootComplete = false;
unsigned long lastHeapLog = 0;
uint32_t minHeapSeen = UINT32_MAX;

void waitForBoot() {
    DEBUG_PRINTLN("[Main] Aguardando estabilização dos subsistemas...");
    for (int i = 3; i > 0; i--) {
        DEBUG_PRINTF("[Main] Iniciando em %d...\n", i);
        uint32_t currentHeap = ESP.getFreeHeap();
        if (currentHeap < minHeapSeen) minHeapSeen = currentHeap;
        delay(1000);
    }
    DEBUG_PRINTF("[Main] Sistema pronto! Heap mínimo no boot: %lu bytes\n", minHeapSeen);
    DEBUG_PRINTLN("");
}

void processSerialCommands() {
    if (!Serial.available()) return;
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd.length() == 0) return;
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTF("[Main] ┌─ Comando recebido: '%s'\n", cmd.c_str());
    
    OperationMode currentMode = telemetry.getMode();
    
    if (cmd == "START" || cmd == "S" || cmd == "1") {
        if (currentMode == MODE_PREFLIGHT) {
            DEBUG_PRINTLN("[Main] │");
            DEBUG_PRINTLN("[Main] └─► INICIANDO MISSÃO");
            telemetry.startMission();
            missionStarted = true;
        } else {
            DEBUG_PRINTF("[Main] └─✗ Erro: Sistema não está em PRE-FLIGHT (modo atual: %d)\n", currentMode);
        }
    } 
    else if (cmd == "STOP" || cmd == "P" || cmd == "0") {
        if (currentMode == MODE_FLIGHT) {
            DEBUG_PRINTLN("[Main] │");
            DEBUG_PRINTLN("[Main] └─► PARANDO MISSÃO");
            telemetry.stopMission();
            missionStarted = false;
        } else {
            DEBUG_PRINTF("[Main] └─✗ Erro: Sistema não está em FLIGHT (modo atual: %d)\n", currentMode);
        }
    }
    else if (cmd == "LORA ON") {
        telemetry.enableLoRa(true);
        DEBUG_PRINTLN("[Main] └─► LoRa HABILITADO");
    }
    else if (cmd == "LORA OFF") {
        telemetry.enableLoRa(false);
        DEBUG_PRINTLN("[Main] └─► LoRa DESABILITADO");
    }
    else if (cmd == "HTTP ON") {
        telemetry.enableHTTP(true);
        DEBUG_PRINTLN("[Main] └─► HTTP HABILITADO");
    }
    else if (cmd == "HTTP OFF") {
        telemetry.enableHTTP(false);
        DEBUG_PRINTLN("[Main] └─► HTTP DESABILITADO");
    }
    else if (cmd.startsWith("MODE ")) {
        if (cmd.indexOf("FLIGHT") > 0) {
            telemetry.applyModeConfig(MODE_FLIGHT);
            DEBUG_PRINTLN("[Main] └─► Modo FLIGHT ativado!");
        } else if (cmd.indexOf("PREFLIGHT") > 0) {
            telemetry.applyModeConfig(MODE_PREFLIGHT);
            DEBUG_PRINTLN("[Main] └─► Modo PREFLIGHT ativado!");
        } else if (cmd.indexOf("SAFE") > 0) {
            telemetry.applyModeConfig(MODE_SAFE);
            DEBUG_PRINTLN("[Main] └─► Modo SAFE ativado!");
        } else {
            DEBUG_PRINTLN("[Main] └─✗ Argumento inválido. Use FLIGHT/PREFLIGHT/SAFE.");
        }
    }
    else if (cmd == "STATUS" || cmd == "?" || cmd == "INFO") {
        DEBUG_PRINTLN("[Main] │");
        DEBUG_PRINTLN("[Main] ╔════════════════════════════════════════╗");
        DEBUG_PRINTLN("[Main] ║        STATUS DO SISTEMA               ║");
        DEBUG_PRINTLN("[Main] ╠════════════════════════════════════════╣");
        
        String modeStr;
        switch(currentMode) {
            case MODE_PREFLIGHT: modeStr = "PRE-FLIGHT"; break;
            case MODE_FLIGHT: modeStr = "FLIGHT"; break;
            case MODE_POSTFLIGHT: modeStr = "POST-FLIGHT"; break;
            case MODE_SAFE: modeStr = "SAFE"; break;
            case MODE_ERROR: modeStr = "ERRO"; break;
            default: modeStr = "DESCONHECIDO"; break;
        }
        
        DEBUG_PRINTF("[Main] ║ Modo:    %-29s ║\n", modeStr.c_str());
        DEBUG_PRINTF("[Main] ║ Uptime:  %lu segundos%-16s║\n", millis()/1000, "");
        DEBUG_PRINTF("[Main] ║ Heap:    %lu KB%-22s║\n", ESP.getFreeHeap()/1024, "");
        DEBUG_PRINTF("[Main] ║ WiFi:    %-29s ║\n", WiFi.isConnected() ? "Conectado" : "Desconectado");
        DEBUG_PRINTF("[Main] ║ LoRa:    %-29s ║\n", telemetry.isLoRaEnabled() ? "Habilitado" : "Desabilitado");
        DEBUG_PRINTF("[Main] ║ HTTP:    %-29s ║\n", telemetry.isHTTPEnabled() ? "Habilitado" : "Desabilitado");
        DEBUG_PRINTLN("[Main] ╚════════════════════════════════════════╝");
    }
    else if (cmd == "HELP" || cmd == "H" || cmd == "AJUDA") {
        DEBUG_PRINTLN("[Main] │");
        DEBUG_PRINTLN("[Main] ╔════════════════════════════════════════╗");
        DEBUG_PRINTLN("[Main] ║      COMANDOS DISPONÍVEIS              ║");
        DEBUG_PRINTLN("[Main] ╠════════════════════════════════════════╣");
        DEBUG_PRINTLN("[Main] ║ START / S / 1   - Iniciar missão       ║");
        DEBUG_PRINTLN("[Main] ║ STOP  / P / 0   - Parar missão         ║");
        DEBUG_PRINTLN("[Main] ║ MODE FLIGHT     - Modo eficiente       ║");
        DEBUG_PRINTLN("[Main] ║ MODE PREFLIGHT  - Debug completo       ║");
        DEBUG_PRINTLN("[Main] ║ MODE SAFE       - Modo degradado       ║");
        DEBUG_PRINTLN("[Main] ║ LORA ON/OFF     - Controlar LoRa       ║");
        DEBUG_PRINTLN("[Main] ║ HTTP ON/OFF     - Controlar HTTP       ║"); 
        DEBUG_PRINTLN("[Main] ║ STATUS / ?      - Mostrar status       ║");
        DEBUG_PRINTLN("[Main] ║ RESTART / R     - Reiniciar ESP32      ║");
        DEBUG_PRINTLN("[Main] ║ HELP  / H       - Esta mensagem        ║");
        DEBUG_PRINTLN("[Main] ╚════════════════════════════════════════╝");
    }
    else if (cmd == "RESTART" || cmd == "R") {
        DEBUG_PRINTLN("[Main] │");
        DEBUG_PRINTLN("[Main] └─► REINICIANDO SISTEMA");
        delay(1000);
        ESP.restart();
    }
    else {
        DEBUG_PRINTF("[Main] └─✗ Comando desconhecido: '%s'\n", cmd.c_str());
        DEBUG_PRINTLN("[Main]    Digite 'HELP' para lista de comandos");
    }
    
    DEBUG_PRINTLN("");
}


void printPeriodicStatus() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastHeapLog >= 30000) {
        lastHeapLog = currentTime;
        uint32_t currentHeap = ESP.getFreeHeap();
        
        if (currentHeap < minHeapSeen) minHeapSeen = currentHeap;
        
        DEBUG_PRINTLN("");
        DEBUG_PRINTLN("========== STATUS DO SISTEMA ==========");
        DEBUG_PRINTF("Uptime: %lu s\n", currentTime / 1000);
        DEBUG_PRINTF("Modo: %d\n", telemetry.getMode());
        DEBUG_PRINTF("Heap atual: %lu KB\n", currentHeap / 1024);
        DEBUG_PRINTF("Heap mínimo: %lu KB\n", minHeapSeen / 1024);
        
        // NOVO: Alerta para SAFE MODE
        if (currentHeap < 15000) {
            DEBUG_PRINTLN("ALERTA: Heap baixo!");
        }
        if (currentHeap < 5000) {
            DEBUG_PRINTLN("CRÍTICO: Entrando em SAFE MODE!");
            telemetry.applyModeConfig(MODE_SAFE);
        }
        
        DEBUG_PRINTLN("=======================================");
        DEBUG_PRINTLN("");
    }
}

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    delay(1000);
    
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO SISTEMA DE TELEMETRIA");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    
    uint32_t heapBeforeInit = ESP.getFreeHeap();
    minHeapSeen = heapBeforeInit;
    DEBUG_PRINTF("[Main] Heap antes da init: %lu bytes\n", heapBeforeInit);
    
    // ✅ CRÍTICO: Configurar watchdog ANTES de inicializar subsistemas
    DEBUG_PRINTLN("[Main] Configurando Watchdog Timer...");
    esp_task_wdt_init(120, true);  // ✅ 120 SEGUNDOS (suficiente para calibração MPU9250)
    esp_task_wdt_add(NULL);
    DEBUG_PRINTLN("[Main] Watchdog habilitado: 120 segundos");
    
    // ✅ Reset antes de começar inicialização
    esp_task_wdt_reset();
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
        DEBUG_PRINTLN("[Main] Sistema continuará em modo degradado...");
    }
    
    // ✅ Reset após inicialização
    esp_task_wdt_reset();
    
    uint32_t heapAfterInit = ESP.getFreeHeap();
    if (heapAfterInit < minHeapSeen) {
        minHeapSeen = heapAfterInit;
    }
    
    DEBUG_PRINTF("[Main] Heap após init: %lu bytes (usado: %lu bytes)\n", 
                 heapAfterInit, heapBeforeInit - heapAfterInit);
    
    DEBUG_PRINTLN("[Main] Aguardando estabilização dos subsistemas...");
    for (int i = 3; i > 0; i--) {
        DEBUG_PRINTF("[Main] Iniciando em %d...\n", i);
        esp_task_wdt_reset();
        delay(1000);
    }
    
    DEBUG_PRINTF("[Main] Sistema pronto! Heap mínimo no boot: %lu bytes\n", minHeapSeen);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] SISTEMA OPERACIONAL");
    DEBUG_PRINTLN("[Main] Modo: PRE-FLIGHT");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] >>> DIGITE 'HELP' PARA LISTA DE COMANDOS <<<");
    DEBUG_PRINTLN("");
}


void loop() {
    esp_task_wdt_reset();
    
    // Monitorar heap para detecção precoce de problemas
    uint32_t loopHeap = ESP.getFreeHeap();
    
    if (loopHeap < minHeapSeen) {
        minHeapSeen = loopHeap;
    }
    
    // ========== PROTEÇÃO CONTRA HEAP CRÍTICO ==========
    if (loopHeap < 8000) {
        DEBUG_PRINTF("[Main] ⚠️  Heap crítico: %lu bytes - Ativando SAFE MODE\n", loopHeap);
        telemetry.applyModeConfig(MODE_SAFE);
    }
    
    // ========== PROTEÇÃO CONTRA CRASH IMINENTE ==========
    if (loopHeap < 5000) { 
        DEBUG_PRINTF("[Main] 🚨 CRÍTICO: Heap muito baixo: %lu bytes\n", loopHeap); 
        DEBUG_PRINTLN("[Main] Sistema será reiniciado em 3 segundos..."); 
        delay(3000); 
        ESP.restart(); 
    }
    
    // ========== LOOP PRINCIPAL DE TELEMETRIA ==========
    telemetry.loop();
    
    // ========== PROCESSAMENTO DE COMANDOS SERIAL ==========
    processSerialCommands();
    
    // ========== STATUS PERIÓDICO ==========
    printPeriodicStatus();

    // ========== CONTROLE DE LED (INDICADOR DE MODO) ==========
    static unsigned long lastBlink = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastBlink >= 1000) {
        lastBlink = currentTime;
        OperationMode currentMode = telemetry.getMode();
        
        switch (currentMode) {
            case MODE_PREFLIGHT: 
                // LED constante em PRE-FLIGHT
                digitalWrite(LED_BUILTIN, HIGH); 
                break;
                
            case MODE_FLIGHT: 
                // LED piscando 1Hz em FLIGHT
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
                break;
                
            case MODE_SAFE:
                // LED piscando rápido (3s on, 2s off) em SAFE
                if ((currentTime / 1000) % 5 < 3) {
                    digitalWrite(LED_BUILTIN, HIGH);
                } else {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                break;
                
            case MODE_POSTFLIGHT:
                // LED piscando 0.5Hz em POST-FLIGHT
                if ((currentTime / 1000) % 2 == 0) {
                    digitalWrite(LED_BUILTIN, HIGH);
                } else {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                break;
                
            case MODE_ERROR:
                // LED piscando muito rápido em ERROR
                if ((currentTime / 100) % 2 == 0) {
                    digitalWrite(LED_BUILTIN, HIGH);
                } else {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                break;
                
            default: 
                digitalWrite(LED_BUILTIN, LOW); 
                break;
        }
    }
    
    // Pequeno delay para estabilidade
    delay(10);
}
