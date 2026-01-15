/**
 * @file Globals.h
 * @brief Gerenciador de recursos globais (Singleton)
 * @version 2.1.0
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ========== DECLARAÇÕES EXTERNAS (compatibilidade) ==========
// Mutexes
extern SemaphoreHandle_t xSerialMutex;
extern SemaphoreHandle_t xI2CMutex;
extern SemaphoreHandle_t xDataMutex;

// Semáforos
extern SemaphoreHandle_t xLoRaRxSemaphore;

// Filas
extern QueueHandle_t xHttpQueue;
extern QueueHandle_t xStorageQueue;

// Flag de logs
extern bool currentSerialLogsEnabled;

// ========== FUNÇÕES GLOBAIS ==========
void initGlobalResources();

// ========== CLASSE RESOURCE MANAGER (SINGLETON) ==========
class ResourceManager {
public:
    static ResourceManager& instance() {
        static ResourceManager inst;
        return inst;
    }
    
    // Inicialização
    bool begin();
    
    // Getters para mutexes
    SemaphoreHandle_t getSerialMutex() const { return _serialMutex; }
    SemaphoreHandle_t getI2CMutex() const { return _i2cMutex; }
    SemaphoreHandle_t getDataMutex() const { return _dataMutex; }
    
    // Getters para semáforos
    SemaphoreHandle_t getLoRaRxSemaphore() const { return _loraRxSemaphore; }
    
    // Getters para filas
    QueueHandle_t getHttpQueue() const { return _httpQueue; }
    QueueHandle_t getStorageQueue() const { return _storageQueue; }
    
    // Status
    bool isInitialized() const { return _initialized; }
    
private:
    ResourceManager() : _initialized(false) {}
    ~ResourceManager() = default;
    
    // Prevent copying
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    
    // Mutexes
    SemaphoreHandle_t _serialMutex = NULL;
    SemaphoreHandle_t _i2cMutex = NULL;
    SemaphoreHandle_t _dataMutex = NULL;
    
    // Semáforos
    SemaphoreHandle_t _loraRxSemaphore = NULL;
    
    // Filas
    QueueHandle_t _httpQueue = NULL;
    QueueHandle_t _storageQueue = NULL;
    
    bool _initialized;
};

#endif // GLOBALS_H
