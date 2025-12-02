#include "CommandHandler.h"
#include "config.h" // Importante para as macros de Debug

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

    // Comandos de Calibração
    if (cmd == "CALIB_MAG") {
        DEBUG_PRINTLN("[CMD] Iniciando calibração Magnetômetro...");
        if (_sensors.recalibrateMagnetometer()) {
            DEBUG_PRINTLN("[CMD] Calibração MAG SUCESSO!");
        } else {
            DEBUG_PRINTLN("[CMD] Calibração MAG FALHOU.");
        }
        return true;
    }

    if (cmd == "CLEAR_MAG") {
        _sensors.clearMagnetometerCalibration();
        return true;
    }

    if (cmd == "SAVE_BASELINE") {
        // Correção de sintaxe: uso obrigatório de chaves com macros
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
    DEBUG_PRINTLN("--- COMANDOS DISPONÍVEIS ---");
    DEBUG_PRINTLN("  STATUS          : Mostra status detalhado dos sensores");
    DEBUG_PRINTLN("  CALIB_MAG       : Recalibra magnetômetro (gire em 8)");
    DEBUG_PRINTLN("  CLEAR_MAG       : Apaga calibração do magnetômetro");
    DEBUG_PRINTLN("  SAVE_BASELINE   : Salva baseline do CCS811 (Ar Limpo)");
    DEBUG_PRINTLN("----------------------------");
}