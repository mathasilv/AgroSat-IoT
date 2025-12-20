/**
 * @file Globals.h
 * @brief Recursos Globais do FreeRTOS
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// Semáforos (Mutexes)
extern SemaphoreHandle_t xSerialMutex;
extern SemaphoreHandle_t xI2CMutex;
extern SemaphoreHandle_t xSPIMutex;
extern SemaphoreHandle_t xDataMutex;

// Semáforo de Interrupção (Binário)
extern SemaphoreHandle_t xLoRaRxSemaphore; // NOVO FASE 5

// Filas
extern QueueHandle_t xHttpQueue;
extern QueueHandle_t xStorageQueue;

// Inicialização
void initGlobalResources();

// Wrappers de Debug
void safePrintln(const char* msg);
void safePrintf(const char* format, ...);

#endif