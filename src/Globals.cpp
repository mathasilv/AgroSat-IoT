#include "Globals.h"
#include "config.h" 
#include <stdarg.h> 
#include <stdio.h>  

SemaphoreHandle_t xSerialMutex = NULL;
SemaphoreHandle_t xI2CMutex = NULL;
SemaphoreHandle_t xSPIMutex = NULL;
SemaphoreHandle_t xDataMutex = NULL;

// NOVO FASE 5: Semáforo para sinalizar chegada de pacote LoRa (ISR -> Task)
SemaphoreHandle_t xLoRaRxSemaphore = NULL;

QueueHandle_t xHttpQueue = NULL;
QueueHandle_t xStorageQueue = NULL;

void initGlobalResources() {
    // Cria Mutexes
    xSerialMutex = xSemaphoreCreateMutex();
    xI2CMutex = xSemaphoreCreateMutex();
    xSPIMutex = xSemaphoreCreateMutex();
    xDataMutex = xSemaphoreCreateMutex();

    // Cria Semáforo Binário para Interrupção
    xLoRaRxSemaphore = xSemaphoreCreateBinary();

    // Filas
    xHttpQueue = xQueueCreate(5, sizeof(HttpQueueMessage));
    xStorageQueue = xQueueCreate(10, sizeof(StorageQueueMessage));

    if (xSerialMutex == NULL || xI2CMutex == NULL || xDataMutex == NULL ||
        xHttpQueue == NULL || xStorageQueue == NULL || xLoRaRxSemaphore == NULL) {
        
        // Falha crítica
        Serial.println("ERRO CRITICO: Falha ao criar recursos RTOS");
        ESP.restart();
    }
}

void safePrintln(const char* msg) {
    if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        Serial.println(msg);
        xSemaphoreGive(xSerialMutex);
    }
}

void safePrintf(const char* format, ...) {
    if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        char loc_buf[256]; 
        
        va_list args;
        va_start(args, format);
        vsnprintf(loc_buf, sizeof(loc_buf), format, args);
        va_end(args);
        
        Serial.print(loc_buf);
        xSemaphoreGive(xSerialMutex);
    }
}