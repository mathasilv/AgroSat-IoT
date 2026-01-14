/**
 * @file Globals.cpp
 * @brief Implementação do gerenciador de recursos globais
 * @version 2.0.0
 */

#include "Globals.h"
#include "types/TelemetryTypes.h"
#include <stdarg.h>

// ========== VARIÁVEIS GLOBAIS (compatibilidade) ==========
SemaphoreHandle_t xSerialMutex = NULL;
SemaphoreHandle_t xI2CMutex = NULL;
SemaphoreHandle_t xSPIMutex = NULL;
SemaphoreHandle_t xDataMutex = NULL;
SemaphoreHandle_t xLoRaRxSemaphore = NULL;
QueueHandle_t xHttpQueue = NULL;
QueueHandle_t xStorageQueue = NULL;

bool currentSerialLogsEnabled = true;

// ========== FUNÇÃO DE INICIALIZAÇÃO (compatibilidade) ==========
void initGlobalResources() {
    ResourceManager::instance().begin();
    
    // Copiar referências para variáveis globais (compatibilidade)
    xSerialMutex = ResourceManager::instance().getSerialMutex();
    xI2CMutex = ResourceManager::instance().getI2CMutex();
    xSPIMutex = ResourceManager::instance().getSPIMutex();
    xDataMutex = ResourceManager::instance().getDataMutex();
    xLoRaRxSemaphore = ResourceManager::instance().getLoRaRxSemaphore();
    xHttpQueue = ResourceManager::instance().getHttpQueue();
    xStorageQueue = ResourceManager::instance().getStorageQueue();
}

// ========== PRINTF THREAD-SAFE ==========
void safePrintf(const char* format, ...) {
    if (xSerialMutex == NULL) return;
    
    if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        va_list args;
        va_start(args, format);
        
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        Serial.print(buffer);
        
        va_end(args);
        xSemaphoreGive(xSerialMutex);
    }
}

// ========== IMPLEMENTAÇÃO DO RESOURCE MANAGER ==========
bool ResourceManager::begin() {
    if (_initialized) return true;
    
    // Criar mutexes
    _serialMutex = xSemaphoreCreateMutex();
    _i2cMutex = xSemaphoreCreateMutex();
    _spiMutex = xSemaphoreCreateMutex();
    _dataMutex = xSemaphoreCreateMutex();
    
    // Criar semáforo binário para LoRa RX
    _loraRxSemaphore = xSemaphoreCreateBinary();
    
    // Criar filas
    _httpQueue = xQueueCreate(5, sizeof(HttpQueueMessage));
    _storageQueue = xQueueCreate(10, sizeof(uint8_t));  // Apenas sinal
    
    // Verificar se todos foram criados
    if (_serialMutex == NULL || _i2cMutex == NULL || _spiMutex == NULL ||
        _dataMutex == NULL || _loraRxSemaphore == NULL ||
        _httpQueue == NULL || _storageQueue == NULL) {
        
        Serial.println("[ResourceManager] ERRO CRITICO: Falha ao criar recursos!");
        delay(1000);
        ESP.restart();
        return false;
    }
    
    _initialized = true;
    return true;
}

