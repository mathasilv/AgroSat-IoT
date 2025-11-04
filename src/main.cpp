/**
 * @file main.cpp
 * @brief Programa principal do CubeSat AgroSat-IoT - OBSAT Fase 2
 * @version 2.0.1
 * @date 2025-11-01
 * 
 * @author Equipe Orbitalis
 * @institution Universidade Federal de Goiás
 * @category N3
 * 
 * @description
 * Sistema de telemetria completo para CubeSat, implementando todos os
 * requisitos da OBSAT Fase 2:
 * - Telemetria via WiFi (HTTP POST JSON) a cada 4 minutos
 * - Sensores: MPU6050 (IMU), BMP280 (Pressão/Temp), Bateria
 * - Armazenamento em SD Card
 * - Payload customizado: Missão AgroSat-IoT (LoRa)
 * - Watchdog e monitoramento de saúde
 * - Display OLED com informações em tempo real
 * 
 * Hardware: LoRa32 V2.1_1.6 (ESP32 + LoRa + OLED)
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

// ============================================================================
// INTERRUPÇÕES
// ============================================================================

/**
 * @brief ISR do botão (Boot button)
 * Usado para iniciar/parar missão manualmente
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
 * @brief Exibe informações de boot no Serial
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
    DEBUG_PRINTF("║ RAM:         %lu KB                                      ║\n", 
                 ESP.getFreeHeap() / 1024);
    DEBUG_PRINTF("║ Flash:       %lu MB                                      ║\n", 
                 ESP.getFlashChipSize() / (1024 * 1024));
    DEBUG_PRINTLN("╠════════════════════════════════════════════════════════════╣");
    DEBUG_PRINTLN("║ Hardware:    LoRa32 V2.1_1.6                              ║");
    DEBUG_PRINTLN("║ Sensores:    MPU6050 (IMU), BMP280 (Pressão/Temp)        ║");
    DEBUG_PRINTLN("║ Comunicação: WiFi (HTTP), LoRa 433MHz                     ║");
    DEBUG_PRINTLN("║ Armazenamento: SD Card                                    ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
}

/**
 * @brief Exibe informações da missão
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
    DEBUG_PRINTLN("║   - Pressione e segure (3s) para modo pré-voo             ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════════════╝");
    DEBUG_PRINTLN("");
}

/**
 * @brief Aguarda inicialização completa
 */
void waitForBoot() {
    DEBUG_PRINTLN("[Main] Aguardando estabilização dos subsistemas...");
    
    for (int i = 3; i > 0; i--) {
        DEBUG_PRINTF("[Main] Iniciando em %d...\n", i);
        delay(1000);
    }
    
    DEBUG_PRINTLN("[Main] Sistema pronto!");
    DEBUG_PRINTLN("");
}

/**
 * @brief Processa comando do botão
 */
void processButton() {
    if (!buttonPressed) return;
    
    buttonPressed = false;
    
    DEBUG_PRINTLN("[Main] Botão pressionado!");
    
    // Verificar modo atual
    OperationMode mode = telemetry.getMode();
    
    if (mode == MODE_PREFLIGHT) {
        // Iniciar missão
        DEBUG_PRINTLN("[Main] >>> INICIANDO MISSÃO <<<");
        telemetry.startMission();
        missionStarted = true;
        
    } else if (mode == MODE_FLIGHT) {
        // Parar missão manualmente
        DEBUG_PRINTLN("[Main] >>> PARANDO MISSÃO <<<");
        telemetry.stopMission();
        missionStarted = false;
        
    } else if (mode == MODE_POSTFLIGHT) {
        // Reiniciar sistema
        DEBUG_PRINTLN("[Main] >>> REINICIANDO SISTEMA <<<");
        delay(1000);
        ESP.restart();
    }
}

/**
 * @brief Exibe status periodicamente no Serial
 */
void printPeriodicStatus() {
    static unsigned long lastStatusPrint = 0;
    unsigned long currentTime = millis();
    
    // A cada 30 segundos
    if (currentTime - lastStatusPrint >= 30000) {
        lastStatusPrint = currentTime;
        
        DEBUG_PRINTLN("");
        DEBUG_PRINTLN("========== STATUS DO SISTEMA ==========");
        DEBUG_PRINTF("Uptime: %lu s\n", currentTime / 1000);
        DEBUG_PRINTF("Modo: %d\n", telemetry.getMode());
        DEBUG_PRINTF("Memória livre: %lu KB\n", ESP.getFreeHeap() / 1024);
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
    
    // Inicializar gerenciador de telemetria
    if (!telemetry.begin()) {
        DEBUG_PRINTLN("[Main] ERRO CRÍTICO: Falha na inicialização!");
        DEBUG_PRINTLN("[Main] Sistema entrará em modo de erro.");
        
        // Piscar LED rapidamente indicando erro
        while (true) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    }
    
    // Aguardar estabilização
    waitForBoot();
    
    bootComplete = true;
    
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("[Main] SISTEMA OPERACIONAL");
    DEBUG_PRINTLN("[Main] Modo: PRE-FLIGHT");
    DEBUG_PRINTLN("[Main] Aguardando comando para iniciar missão");
    DEBUG_PRINTLN("[Main] ========================================");
    DEBUG_PRINTLN("");
    
    // LED aceso indicando sistema pronto
    digitalWrite(LED_BUILTIN, HIGH);
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================

void loop() {
    // Executar loop de telemetria
    telemetry.loop();
    
    // Processar botão
    processButton();
    
    // Exibir status periódico
    printPeriodicStatus();
    
    // Piscar LED durante operação
    static unsigned long lastBlink = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastBlink >= 1000) {
        lastBlink = currentTime;
        
        if (telemetry.getMode() == MODE_FLIGHT) {
            // Piscar rápido durante voo
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        } else {
            // LED aceso em pré-voo
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
}
