/**
 * @file main.cpp
 * @brief Programa principal - CONTROLE VIA BOTÃO FÍSICO
 * @version 5.0.0
 * @date 2025-11-24
 */
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "TelemetryManager.h"

TelemetryManager telemetry;
bool bootComplete = false;
unsigned long lastHeapLog = 0;
uint32_t minHeapSeen = UINT32_MAX;

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
    
    // Configurar botão no GPIO 4
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO SISTEMA DE TELEMETRIA");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTF("[Main] Botão de controle: GPIO %d\n", BUTTON_PIN);
    DEBUG_PRINTLN("[Main]   - Pressionar: Toggle PREFLIGHT ↔ FLIGHT");
    DEBUG_PRINTLN("[Main]   - Segurar 3s: Ativar SAFE MODE");
    DEBUG_PRINTLN("");
    
    uint32_t heapBeforeInit = ESP.getFreeHeap();
    minHeapSeen = heapBeforeInit;
    DEBUG_PRINTF("[Main] Heap antes da init: %lu bytes\n", heapBeforeInit);
    
    // Configurar watchdog
    DEBUG_PRINTLN("[Main] Configurando Watchdog Timer...");
    esp_task_wdt_init(120, true);
    esp_task_wdt_add(NULL);
    DEBUG_PRINTLN("[Main] Watchdog habilitado: 120 segundos");
    
    esp_task_wdt_reset();
    
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
        DEBUG_PRINTLN("[Main] Sistema continuará em modo degradado...");
    }
    
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
    DEBUG_PRINTLN("[Main] Controle via BOTÃO GPIO 4");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
}

void loop() {
    esp_task_wdt_reset();
    
    // Monitorar heap
    uint32_t loopHeap = ESP.getFreeHeap();
    
    if (loopHeap < minHeapSeen) {
        minHeapSeen = loopHeap;
    }
    
    // Proteção heap crítico
    if (loopHeap < 8000) {
        DEBUG_PRINTF("[Main] Heap crítico: %lu bytes - Ativando SAFE MODE\n", loopHeap);
        telemetry.applyModeConfig(MODE_SAFE);
    }
    
    // Proteção contra crash
    if (loopHeap < 5000) { 
        DEBUG_PRINTF("[Main] CRÍTICO: Heap muito baixo: %lu bytes\n", loopHeap); 
        DEBUG_PRINTLN("[Main] Sistema será reiniciado em 3 segundos..."); 
        delay(3000); 
        ESP.restart(); 
    }
    
    // Loop principal de telemetria (inclui processamento de botão)
    telemetry.loop();
    
    // Status periódico
    printPeriodicStatus();

    // Controle de LED (indicador de modo)
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
                
            case MODE_SAFE:
                if ((currentTime / 1000) % 5 < 3) {
                    digitalWrite(LED_BUILTIN, HIGH);
                } else {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                break;
                
            case MODE_POSTFLIGHT:
                if ((currentTime / 1000) % 2 == 0) {
                    digitalWrite(LED_BUILTIN, HIGH);
                } else {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                break;
                
            case MODE_ERROR:
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
    
    delay(10);
}
