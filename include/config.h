/**
 * @file config.h
 * @brief Configuração central do sistema AgroSat-IoT
 * 
 * @details Arquivo de configuração principal que agrega todos os módulos
 *          de configuração do sistema. Organizado de forma modular para
 *          facilitar manutenção e personalização.
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 2.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Estrutura de Configuração
 * ```
 * include/
 * ├── config.h              <- Este arquivo (agregador)
 * ├── config/
 * │   ├── pins.h            <- Definições de pinos GPIO
 * │   ├── constants.h       <- Constantes e limites do sistema
 * │   ├── modes.h           <- Configurações de modos de operação
 * │   └── debug.h           <- Macros de debug e logging
 * ├── types/
 * │   └── TelemetryTypes.h  <- Estruturas de dados
 * └── Globals.h             <- Recursos globais (mutexes, filas)
 * ```
 * 
 * ## Uso
 * @code{.cpp}
 * #include "config.h"  // Inclui toda a configuração
 * @endcode
 * 
 * @see config/pins.h para mapeamento de hardware
 * @see config/constants.h para ajuste de parâmetros
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

//=============================================================================
// MÓDULOS DE CONFIGURAÇÃO
//=============================================================================
#include "config/pins.h"
#include "config/constants.h"
#include "config/modes.h"
#include "config/debug.h"
#include "types/TelemetryTypes.h"

//=============================================================================
// RECURSOS GLOBAIS (Mutexes, Filas, Semáforos)
//=============================================================================
#include "Globals.h"

#endif // CONFIG_H