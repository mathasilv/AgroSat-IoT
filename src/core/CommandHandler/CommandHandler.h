/**
 * @file CommandHandler.h
 * @brief Processador de comandos do sistema
 * @version 1.0.0
 * @date 2025-12-01
 * 
 * Responsabilidades:
 * - Processar comandos recebidos via Serial/LoRa
 * - Delegar ações para subsistemas apropriados
 * - Exibir menu de ajuda
 */

#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include "sensors/SensorManager/SensorManager.h"

class CommandHandler {
public:
    /**
     * @brief Construtor
     * @param sensors Referência ao gerenciador de sensores
     */
    CommandHandler(SensorManager& sensors);
    
    /**
     * @brief Processa um comando
     * @param cmd String do comando (ex: "STATUS", "HELP")
     * @return true se comando foi reconhecido, false caso contrário
     */
    bool handle(const String& cmd);
    
private:
    SensorManager& _sensors;
    
    /**
     * @brief Processa comandos relacionados a sensores
     * @param cmd Comando a processar
     * @return true se foi comando de sensor
     */
    bool _handleSensorCommands(const String& cmd);
    
    /**
     * @brief Processa comandos do CCS811
     * @param cmd Comando a processar
     * @return true se foi comando CCS811
     */
    bool _handleCCS811Commands(const String& cmd);
    
    /**
     * @brief Processa comandos de sistema
     * @param cmd Comando a processar
     * @return true se foi comando de sistema
     */
    bool _handleSystemCommands(const String& cmd);
    
    /**
     * @brief Exibe menu de ajuda completo
     */
    void _printHelpMenu();
};

#endif // COMMAND_HANDLER_H
