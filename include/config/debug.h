/**
 * @file debug.h
 * @brief Macros e funções de debug thread-safe para FreeRTOS
 * 
 * @details Implementa sistema de logging seguro para ambiente multi-task:
 *          - Proteção de mutex para acesso à Serial
 *          - Timeout de 100ms para evitar deadlocks
 *          - Flag global para habilitar/desabilitar logs
 *          - Suporte a print, println e printf
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Macros Disponíveis
 * | Macro           | Uso                              |
 * |-----------------|----------------------------------|
 * | DEBUG_PRINT(x)  | Imprime sem newline              |
 * | DEBUG_PRINTLN(x)| Imprime com newline              |
 * | DEBUG_PRINTF(.) | Printf formatado                 |
 * 
 * ## Exemplo de Uso
 * @code{.cpp}
 * DEBUG_PRINTLN("[Module] Iniciando...");
 * DEBUG_PRINTF("[Module] Valor: %d, Float: %.2f\n", intVal, floatVal);
 * @endcode
 * 
 * ## Thread Safety
 * - Todas as macros usam mutex xSerialMutex
 * - Timeout de 100ms previne deadlocks
 * - Se mutex não disponível, mensagem é descartada
 * 
 * @note Logs são controlados por currentSerialLogsEnabled
 * @note Em modo FLIGHT, logs são desabilitados automaticamente
 * @see modes.h para configuração de logs por modo
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

//=============================================================================
// DECLARAÇÕES EXTERNAS
//=============================================================================

extern SemaphoreHandle_t xSerialMutex;  ///< Mutex para acesso à Serial
extern bool currentSerialLogsEnabled;   ///< Flag global de habilitação de logs

//=============================================================================
// FUNÇÕES DE DEBUG THREAD-SAFE
//=============================================================================

/**
 * @brief Imprime valor na Serial com proteção de mutex
 * @tparam T Tipo do valor (int, float, String, etc.)
 * @param val Valor a ser impresso
 * @note Timeout de 100ms - descarta se mutex ocupado
 */
template <typename T>
inline void _debugPrintSafe(T val) {
    if (xSerialMutex != NULL) {
        if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.print(val);
            xSemaphoreGive(xSerialMutex);
        }
    }
}

/**
 * @brief Imprime valor na Serial com newline e proteção de mutex
 * @tparam T Tipo do valor (int, float, String, etc.)
 * @param val Valor a ser impresso
 * @note Timeout de 100ms - descarta se mutex ocupado
 */
template <typename T>
inline void _debugPrintlnSafe(T val) {
    if (xSerialMutex != NULL) {
        if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.println(val);
            xSemaphoreGive(xSerialMutex);
        }
    }
}

/**
 * @brief Printf thread-safe com proteção de mutex
 * @param format String de formato (estilo printf)
 * @param ... Argumentos variádicos
 * @note Implementado em Globals.cpp
 * @note Buffer interno de 256 bytes
 */
void safePrintf(const char* format, ...);

//=============================================================================
// MACROS DE DEBUG
//=============================================================================

/**
 * @def DEBUG_PRINT(x)
 * @brief Imprime valor sem newline (se logs habilitados)
 * @param x Valor a imprimir
 */
#define DEBUG_PRINT(x) \
    do { if(currentSerialLogsEnabled) { _debugPrintSafe(x); } } while(0)

/**
 * @def DEBUG_PRINTLN(x)
 * @brief Imprime valor com newline (se logs habilitados)
 * @param x Valor a imprimir
 */
#define DEBUG_PRINTLN(x) \
    do { if(currentSerialLogsEnabled) { _debugPrintlnSafe(x); } } while(0)

/**
 * @def DEBUG_PRINTF(...)
 * @brief Printf formatado (se logs habilitados)
 * @param ... Formato e argumentos estilo printf
 */
#define DEBUG_PRINTF(...) \
    do { if(currentSerialLogsEnabled) { safePrintf(__VA_ARGS__); } } while(0)

#endif // DEBUG_H