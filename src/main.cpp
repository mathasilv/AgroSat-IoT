/**
 * @file main.cpp
 * @brief Programa principal robusto do CubeSat AgroSat-IoT - OBSAT Fase 2
 * @version 2.1.0
 * @date 2025-11-01
 */

#include <Arduino.h>
#include "config.h"
#include "TelemetryManager.h"

// ============================================================================
// VARIÁVEIS GLOBAIS
// ============================================================================

TelemetryManager telemetry;

// Flags de controle
bool missionStarted = false;
bool bootComplete = false;

// Botão de controle
volatile bool buttonPressed = false;
unsigned long lastButtonPress = 0;

// Monitoramento de sistema
unsigned long lastHeapLog = 0;
uint32_t minHeapSeen = UINT32_MAX;

// ============================================================================
// INTERRUPÇÕES
// ============================================================================

/**
 * @brief ISR do botão (Boot button) - Mantido leve e seguro
 */
void IRAM_ATTR buttonISR() {
    unsigned long currentTime = millis();
    
    // Debounce (200ms)
    if (currentTime - lastButtonPress > 200) {
        buttonPressed = true;
        lastButtonPress = currentTime;
    }
}

// ============================================================================
// FUNÇÕES AUXILIARES
// ============================================================================

/**
 * @brief Exibe informações detalhadas de boot
 */
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
    
    // Log inicial de heap para monitoramento
    minHeapSeen = freeHeap;
    
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTLN("║ Hardware:    LoRa32 V2.1_1.6                              ║");
    DEBUG_PRINTLN("║ Sensores:    MPU6050 (IMU), BMP280 (Pressão/Temp)        ║");
    DEBUG_PRINTLN("║ Comunicação: WiFi (HTTP), LoRa 433MHz                     ║");
    DEBUG_PRINTLN("║ Armazenamento: SD Card                                    ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
}

/**
 * @brief Informações da missão e requisitos
 */
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
    DEBUG_PRINTLN("║ Controles:                                                 ║");
    DEBUG_PRINTLN("║   - Pressione BOOT button para iniciar/parar missão       ║");
    DEBUG_PRINTLN("║   - Sistema monitora heap e detecta problemas             ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
}

/**
 * @brief Aguarda boot com verificações de sistema
 */
void waitForBoot() {
    DEBUG_PRINTLN("[Main] Aguardando estabilização dos subsistemas...");
    
    for (int i = 3; i > 0; i--) {
        DEBUG_PRINTF("[Main] Iniciando em %d...\n", i);
        
        // Verificar heap durante boot
        uint32_t currentHeap = ESP.getFreeHeap();
        if (currentHeap < minHeapSeen) {
            minHeapSeen = currentHeap;
        }
        
        delay(1000);
    }
    
    DEBUG_PRINTF("[Main] Sistema pronto! Heap mínimo no boot: %lu bytes\n", minHeapSeen);
    DEBUG_PRINTLN("");
}

/**
 * @brief Processa comando do botão com validações
 */
void processButton() {
    if (!buttonPressed) return;
    
    buttonPressed = false;
    
    DEBUG_PRINTLN("[Main] Botão pressionado!");
    
    // Log de heap no momento do comando
    uint32_t heapAtButton = ESP.getFreeHeap();
    DEBUG_PRINTF("[Main] Heap no comando: %lu bytes\n", heapAtButton);
    
    // Verificar modo atual
    OperationMode mode = telemetry.getMode();
    
    switch (mode) {
        case MODE_PREFLIGHT:
            DEBUG_PRINTLN("[Main] >>> INICIANDO MISSÃO <<<");
            telemetry.startMission();
            missionStarted = true;
            break;
            
        case MODE_FLIGHT:
            DEBUG_PRINTLN("[Main] >>> PARANDO MISSÃO <<<");
            telemetry.stopMission();
            missionStarted = false;
            break;
            
        case MODE_POSTFLIGHT:
            DEBUG_PRINTLN("[Main] >>> REINICIANDO SISTEMA <<<");
            DEBUG_PRINTF("[Main] Heap final antes do restart: %lu bytes\n", ESP.getFreeHeap());
            delay(1000);
            ESP.restart();
            break;
            
        case MODE_ERROR:
            DEBUG_PRINTLN("[Main] Sistema em erro - tentando restart");
            delay(1000);
            ESP.restart();
            break;
            
        default:
            DEBUG_PRINTF("[Main] Modo desconhecido: %d\n", mode);
            break;
    }
}

/**
 * @brief Status periódico com monitoramento robusto
 */
void printPeriodicStatus() {
    unsigned long currentTime = millis();
    
    // A cada 30 segundos
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
        
        // Alertas de heap
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
    // Inicializar Serial para debug
    Serial.begin(DEBUG_BAUDRATE);
    delay(500);  // Aguardar estabilização Serial
    
    // Log de heap inicial do sistema
    uint32_t bootHeap = ESP.getFreeHeap();
    minHeapSeen = bootHeap;
    
    // Exibir informações de boot
    printBootInfo();
    printMissionInfo();
    
    // Configurar LED onboard
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    // Configurar botão com interrupção
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
    
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] INICIALIZANDO SISTEMA DE TELEMETRIA");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    
    // Verificar heap antes da inicialização crítica
    DEBUG_PRINTF("[Main] Heap antes da init: %lu bytes\n", ESP.getFreeHeap());
    
    // Inicializar gerenciador de telemetria
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
        DEBUG_PRINTLN("[Main] Sistema entrará em modo de erro.");
        DEBUG_PRINTF("[Main] Heap no erro: %lu bytes\n", ESP.getFreeHeap());
        
        // Piscar LED rapidamente indicando erro
        while (true) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    }
    
    // Verificar heap após inicialização
    uint32_t postInitHeap = ESP.getFreeHeap();
    DEBUG_PRINTF("[Main] Heap após init: %lu bytes (usado: %lu bytes)\n", 
                postInitHeap, bootHeap - postInitHeap);
    
    if (postInitHeap < minHeapSeen) {
        minHeapSeen = postInitHeap;
    }
    
    // Aguardar estabilização
    waitForBoot();
    
    bootComplete = true;
    
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] SISTEMA OPERACIONAL");
    DEBUG_PRINTLN("[Main] Modo: PRE-FLIGHT");
    DEBUG_PRINTLN("[Main] Aguardando comando para iniciar missão");
    DEBUG_PRINTF("[Main] Heap estabilizado: %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    
    // LED aceso indicando sistema pronto
    digitalWrite(LED_BUILTIN, HIGH);
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================

void loop() {
    // Verificar heap no início do loop (detecção precoce de problemas)
    uint32_t loopHeap = ESP.getFreeHeap();
    if (loopHeap < minHeapSeen) {
        minHeapSeen = loopHeap;
    }
    
    // Detectar heap criticamente baixo
    if (loopHeap < 5000) {
        DEBUG_PRINTF("[Main] CRÍTICO: Heap muito baixo no loop: %lu bytes\n", loopHeap);
        DEBUG_PRINTLN("[Main] Reiniciando sistema para evitar crash...");
        delay(1000);
        ESP.restart();
    }
    
    // Executar loop de telemetria
    telemetry.loop();
    
    // Processar botão
    processButton();
    
    // Exibir status periódico
    printPeriodicStatus();
    
    // Controle de LED baseado no modo
    static unsigned long lastBlink = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastBlink >= 1000) {
        lastBlink = currentTime;
        
        OperationMode currentMode = telemetry.getMode();
        
        switch (currentMode) {
            case MODE_PREFLIGHT:
                // LED aceso em pré-voo
                digitalWrite(LED_BUILTIN, HIGH);
                break;
                
            case MODE_FLIGHT:
                // Piscar rápido durante voo
                digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
                break;
                
            case MODE_POSTFLIGHT:
                // Piscar lento após voo
                static bool slowBlink = false;
                if (currentTime % 2000 < 1000) {
                    digitalWrite(LED_BUILTIN, HIGH);
                } else {
                    digitalWrite(LED_BUILTIN, LOW);
                }
                break;
                
            case MODE_ERROR:
                // Piscar muito rápido em erro
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
