/**
 * @file CommandHandler.cpp
 * @brief Implementação do processador de comandos Serial
 */

#include "CommandHandler.h"
#include "config.h"

CommandHandler::CommandHandler(SensorManager& sensors) : _sensors(sensors) {}

bool CommandHandler::handle(String cmd) {
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd.length() == 0) return false;

    if (cmd == "HELP" || cmd == "?") {
        _printHelp();
        return true;
    }
    
    if (cmd.startsWith("STATUS")) {
        _sensors.printDetailedStatus();
        return true;
    }

    if (cmd == "CALIB_MAG") {
        DEBUG_PRINTLN("[CMD] Iniciando calibracao do magnetometro...");
        if (_sensors.recalibrateMagnetometer()) {
            DEBUG_PRINTLN("[CMD] Calibracao MAG concluida.");
        } else {
            DEBUG_PRINTLN("[CMD] Calibracao MAG falhou.");
        }
        return true;
    }

    if (cmd == "CLEAR_MAG") {
        _sensors.clearMagnetometerCalibration();
        return true;
    }

    if (cmd == "SAVE_BASELINE") {
        if (_sensors.saveCCS811Baseline()) {
            DEBUG_PRINTLN("[CMD] Baseline salvo.");
        } else {
            DEBUG_PRINTLN("[CMD] Erro ao salvar baseline.");
        }
        return true;
    }

    DEBUG_PRINTF("[CMD] Comando desconhecido: %s\n", cmd.c_str());
    return false;
}

void CommandHandler::_printHelp() {
    DEBUG_PRINTLN("--- COMANDOS DISPONIVEIS ---");
    DEBUG_PRINTLN("  STATUS          : Status detalhado dos sensores");
    DEBUG_PRINTLN("  CALIB_MAG       : Calibra magnetometro (gire em 8)");
    DEBUG_PRINTLN("  CLEAR_MAG       : Apaga calibracao do magnetometro");
    DEBUG_PRINTLN("  SAVE_BASELINE   : Salva baseline do CCS811");
    DEBUG_PRINTLN("----------------------------");
}