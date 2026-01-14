/**
 * @file CommandHandler.h
 * @brief Processador de comandos via interface Serial
 * 
 * @details Implementa um interpretador de comandos para controle
 *          e diagnóstico do sistema via terminal Serial:
 *          - Comandos de status e diagnóstico
 *          - Controle de sensores e calibração
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.2.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Comandos Disponíveis
 * | Comando         | Descrição                      |
 * |-----------------|--------------------------------|
 * | HELP            | Lista comandos disponíveis     |
 * | STATUS          | Status detalhado dos sensores  |
 * | CALIB_MAG       | Inicia calibração magnetômetro |
 * | CLEAR_MAG       | Apaga calibração magnetômetro  |
 * | SAVE_BASELINE   | Salva baseline do CCS811       |
 * 
 * @note Comandos são case-insensitive
 * @see TelemetryManager::handleCommand() para comandos de missão
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include "sensors/SensorManager/SensorManager.h"

/**
 * @class CommandHandler
 * @brief Processador de comandos Serial para diagnóstico e controle
 */
class CommandHandler {
public:
    /**
     * @brief Construtor com injeção de dependência
     * @param sensors Referência ao SensorManager para comandos de sensor
     */
    explicit CommandHandler(SensorManager& sensors);
    
    /**
     * @brief Processa um comando recebido
     * @param cmd Comando em formato string (case-insensitive)
     * @return true se comando reconhecido e executado
     * @return false se comando desconhecido
     */
    bool handle(String cmd);

private:
    SensorManager& _sensors;     ///< Referência ao gerenciador de sensores
    
    /** @brief Imprime lista de comandos disponíveis */
    void _printHelp();
};

#endif // COMMAND_HANDLER_H
