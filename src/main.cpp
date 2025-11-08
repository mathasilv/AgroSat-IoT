/**
 * @file main.cpp
 * @brief Programa principal com controle via Serial (SEM botão físico)
 * @version 2.2.0
 */

#include <Arduino.h>
#include "config.h"
#include "TelemetryManager.h"

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

TelemetryManager telemetry;

bool missionStarted = false;
bool bootComplete = false;

// Monitoramento de sistema
unsigned long lastHeapLog = 0;
uint32_t minHeapSeen = UINT32_MAX;

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

void printBootInfo() {
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("╔════════════════════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║           AgroSat-IoT CubeSat - OBSAT Fase 2              ║");
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTF("║ Equipe:      %-45s ║\n", TEAM_NAME);
    DEBUG_PRINTF("║ Categoria:   %-45s ║\n", TEAM_CATEGORY);
    DEBUG_PRINTF("║ Missão:      %-45s ║\n", MISSION_NAME);
    DEBUG_PRINTF("║ Firmware:    %-45s ║\n", FIRMWARE_VERSION);
    DEBUG_PRINTF("║ Build:       %-45s ║\n", BUILD_DATE " " BUILD_TIME);
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTF("║ CPU:         ESP32 @ %lu MHz                             ║\n", 
                 getCpuFrequencyMhz());
    
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    DEBUG_PRINTF("║ RAM:         %lu KB livre / %lu KB total              ║\n", 
                 freeHeap / 1024, totalHeap / 1024);
    DEBUG_PRINTF("║ Flash:       %lu MB                                      ║\n", 
                 ESP.getFlashChipSize() / (1024 * 1024));
    
    minHeapSeen = freeHeap;
    
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTLN("║ Hardware:    LoRa32 V2.1_1.6                              ║");
    DEBUG_PRINTLN("║ Sensores:    MPU6050 (IMU), BMP280 (Pressão/Temp)        ║");
    DEBUG_PRINTLN("║ Comunicação: WiFi (HTTP), LoRa 433MHz                     ║");
    DEBUG_PRINTLN("║ Armazenamento: SD Card                                    ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
}

void printMissionInfo() {
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("╔════════════════════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║                    INFORMAÇÕES DA MISSÃO                   ║");
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTLN("║ Objetivo:                                                  ║");
    DEBUG_PRINTLN("║   Monitoramento remoto de cultivos agrícolas via          ║");
    DEBUG_PRINTLN("║   CubeSat com comunicação LoRa IoT                         ║");
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTLN("║ Requisitos OBSAT Fase 2:                                  ║");
    DEBUG_PRINTLN("║   ✓ Telemetria via WiFi HTTP POST (JSON)                  ║");
    DEBUG_PRINTLN("║   ✓ Intervalo: 4 minutos                                  ║");
    DEBUG_PRINTLN("║   ✓ Duração: 2 horas                                      ║");
    DEBUG_PRINTLN("║   ✓ Dados: Bateria, Temp, Pressão, IMU (6 eixos)         ║");
    DEBUG_PRINTLN("║   ✓ Payload customizado: Dados LoRa (máx 90 bytes)       ║");
    DEBUG_PRINTLN("║   ✓ Armazenamento em SD Card                              ║");
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTLN("║ Controles VIA SERIAL:                                      ║");
    DEBUG_PRINTLN("║   - Digite 'START' para iniciar missão                    ║");
    DEBUG_PRINTLN("║   - Digite 'STOP' para parar missão                       ║");
    DEBUG_PRINTLN("║   - Digite 'HELP' para lista completa                     ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
}

void waitForBoot() {
    DEBUG_PRINTLN("[Main] Aguardando estabilização dos subsistemas...");
    
    for (int i = 3; i > 0; i--) {
        DEBUG_PRINTF("[Main] Iniciando em %d...\n", i);
        
        uint32_t currentHeap = ESP.getFreeHeap();
        if (currentHeap < minHeapSeen) {
            minHeapSeen = currentHeap;
        }
        
        delay(1000);
    }
    
    DEBUG_PRINTF("[Main] Sistema pronto! Heap mínimo no boot: %lu bytes\n", minHeapSeen);
    DEBUG_PRINTLN("");
}

// ============================================================================
// CONTROLE VIA SERIAL (SUBSTITUI BOTÃO)
// ============================================================================

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
        
    } else if (cmd == "STOP" || cmd == "P" || cmd == "0") {
        if (currentMode == MODE_FLIGHT) {
            DEBUG_PRINTLN("[Main] │");
            DEBUG_PRINTLN("[Main] └─► PARANDO MISSÃO");
            telemetry.stopMission();
            missionStarted = false;
        } else {
            DEBUG_PRINTF("[Main] └─✗ Erro: Sistema não está em FLIGHT (modo atual: %d)\n", currentMode);
        }
        
    } else if (cmd == "RESTART" || cmd == "R") {
        DEBUG_PRINTLN("[Main] │");
        DEBUG_PRINTLN("[Main] └─► REINICIANDO SISTEMA");
        DEBUG_PRINTF("[Main]    Heap final: %lu bytes\n", ESP.getFreeHeap());
        delay(1000);
        ESP.restart();
        
    } else if (cmd == "STATUS" || cmd == "?" || cmd == "INFO") {
        DEBUG_PRINTLN("[Main] │");
        DEBUG_PRINTLN("[Main] ╔════════════════════════════════════════╗");
        DEBUG_PRINTLN("[Main] ║        STATUS DO SISTEMA               ║");
        DEBUG_PRINTLN("[Main] ╠════════════════════════════════════════╣");
        
        String modeStr;
        switch(currentMode) {
            case MODE_PREFLIGHT: modeStr = "PRE-FLIGHT"; break;
            case MODE_FLIGHT: modeStr = "FLIGHT"; break;
            case MODE_POSTFLIGHT: modeStr = "POST-FLIGHT"; break;
            case MODE_ERROR: modeStr = "ERRO"; break;
            default: modeStr = "DESCONHECIDO"; break;
        }
        
        DEBUG_PRINTF("[Main] ║ Modo:    %-29s ║\n", modeStr.c_str());
        DEBUG_PRINTF("[Main] ║ Uptime:  %lu segundos%-16s║\n", millis()/1000, "");
        DEBUG_PRINTF("[Main] ║ Heap:    %lu KB%-22s║\n", ESP.getFreeHeap()/1024, "");
        DEBUG_PRINTF("[Main] ║ WiFi:    %-29s ║\n", WiFi.isConnected() ? "Conectado" : "Desconectado");
        DEBUG_PRINTLN("[Main] ╚════════════════════════════════════════╝");
        
    } else if (cmd == "HELP" || cmd == "H" || cmd == "AJUDA") {
        DEBUG_PRINTLN("[Main] │");
        DEBUG_PRINTLN("[Main] ╔════════════════════════════════════════╗");
        DEBUG_PRINTLN("[Main] ║      COMANDOS DISPONÍVEIS              ║");
        DEBUG_PRINTLN("[Main] ╠════════════════════════════════════════╣");
        DEBUG_PRINTLN("[Main] ║ START / S / 1  - Iniciar missão        ║");
        DEBUG_PRINTLN("[Main] ║ STOP  / P / 0  - Parar missão          ║");
        DEBUG_PRINTLN("[Main] ║ STATUS / ?     - Mostrar status        ║");
        DEBUG_PRINTLN("[Main] ║ RESTART / R    - Reiniciar ESP32       ║");
        DEBUG_PRINTLN("[Main] ║ HELP  / H      - Esta mensagem         ║");
        DEBUG_PRINTLN("[Main] ╚════════════════════════════════════════╝");
        
    } else {
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
        if (currentHeap < minHeapSeen) {
            minHeapSeen = currentHeap;
        }
        
        DEBUG_PRINTLN("");
        DEBUG_PRINTLN("========== STATUS DO SISTEMA ==========");
        DEBUG_PRINTF("Uptime: %lu s\n", currentTime / 1000);
        DEBUG_PRINTF("Modo: %d\n", telemetry.getMode());
        DEBUG_PRINTF("Heap atual: %lu KB\n", currentHeap / 1024);
        DEBUG_PRINTF("Heap mínimo: %lu KB\n", minHeapSeen / 1024);
        
        if (currentHeap < 15000) {
            DEBUG_PRINTLN("ALERTA: Heap baixo!");
        }
        
        if (minHeapSeen < 10000) {
            DEBUG_PRINTLN("CRÍTICO: Heap mínimo muito baixo!");
        }
        
        DEBUG_PRINTLN("=======================================");
        DEBUG_PRINTLN("");
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    delay(500);
    
    uint32_t bootHeap = ESP.getFreeHeap();
    minHeapSeen = bootHeap;
    
    printBootInfo();
    printMissionInfo();
    
    // Configurar LED onboard
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // NÃO configurar botão (não existe na placa)
    
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO SISTEMA DE TELEMETRIA");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    
    DEBUG_PRINTF("[Main] Heap antes da init: %lu bytes\n", ESP.getFreeHeap());
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
        DEBUG_PRINTLN("[Main] Sistema entrará em modo de erro.");
        DEBUG_PRINTF("[Main] Heap no erro: %lu bytes\n", ESP.getFreeHeap());
        
        while (true) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    }
    
    uint32_t postInitHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[Main] Heap após init: %lu bytes (usado: %lu bytes)\n", 
                postInitHeap, bootHeap - postInitHeap);
    
    if (postInitHeap < minHeapSeen) {
        minHeapSeen = postInitHeap;
    }
    
    waitForBoot();
    
    bootComplete = true;
    
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] SISTEMA OPERACIONAL");
    DEBUG_PRINTLN("[Main] Modo: PRE-FLIGHT");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] >>> DIGITE 'HELP' PARA LISTA DE COMANDOS <<<");
    DEBUG_PRINTLN("");
    
    digitalWrite(LED_BUILTIN, HIGH);
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================

void loop() {
    uint32_t loopHeap = ESP.getFreeHeap();
    if (loopHeap < minHeapSeen) {
        minHeapSeen = loopHeap;
    }
    
    if (loopHeap < 5000) {
        DEBUG_PRINTF("[Main] CRÍTICO: Heap muito baixo no loop: %lu bytes\n", loopHeap);
        DEBUG_PRINTLN("[Main] Reiniciando sistema para evitar crash...");
        delay(1000);
        ESP.restart();
    }
    
    telemetry.loop();
    
    // SUBSTITUIR processButton() por processSerialCommands()
    processSerialCommands();
    
    printPeriodicStatus();
    
    // Controle de LED baseado no modo
    static unsigned long lastBlink = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastBlink >= 1000) {
        lastBlink = currentTime;
        
        OperationMode currentMode = telemetry.getMode();
        
        switch (currentMode) {
            case MODE_PREFLIGHT:
                digitalWrite(LED_BUILTIN, HIGH);
                break;
                
            case MODE_FLIGHT:
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                break;
                
            case MODE_POSTFLIGHT:
                if (currentTime % 2000 < 1000) {
                    digitalWrite(LED_BUILTIN, HIGH);
                } else {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                break;
                
            case MODE_ERROR:
                if (currentTime % 200 < 100) {
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
}

