/**
 * @file CommandHandler.cpp
 * @brief Implementação do processador de comandos
 * @version 1.0.0
 * @date 2025-12-01
 */

#include "CommandHandler.h"
#include "config.h"

CommandHandler::CommandHandler(SensorManager& sensors) :
    _sensors(sensors)
{
}

bool CommandHandler::handle(const String& cmd) {
    // Tentar cada categoria de comandos
    if (_handleSensorCommands(cmd)) return true;
    if (_handleCCS811Commands(cmd)) return true;
    if (_handleSystemCommands(cmd)) return true;
    
    if (cmd == "TEST_CCS811") {  // Novo comando para teste CCS811
        DEBUG_PRINTLN("[CommandHandler] === TESTE ISOLADO CCS811 ===");
        
        // 1. Testar presença I2C no endereço padrão
        Wire.beginTransmission(0x5A);
        uint8_t ping = Wire.endTransmission();
        DEBUG_PRINTF("Ping 0x5A: %d (0=OK)\n", ping);
        
        Wire.beginTransmission(0x5B);
        ping = Wire.endTransmission();
        DEBUG_PRINTF("Ping 0x5B: %d (0=OK)\n", ping);
        
        // 2. Ler HW_ID
        Wire.beginTransmission(0x5A);
        Wire.write(0x20); // REG_HW_ID
        if (Wire.endTransmission(false) == 0) {
            if (Wire.requestFrom(0x5A, 1) == 1) {
                uint8_t hwId = Wire.read();
                DEBUG_PRINTF("HW_ID 0x5A: 0x%02X (esperado 0x81)\n", hwId);
            }
        }
        
        // 3. Resetar barramento I2C fisicamente via SensorManager
        _sensors.resetAll();
        delay(1000);
        
        DEBUG_PRINTLN("[CommandHandler] Teste CCS811 concluído");
        return true;
    }
    
    // Comando não reconhecido
    DEBUG_PRINTF("[CommandHandler] Comando desconhecido: %s\n", cmd.c_str());
    return false;
}

bool CommandHandler::_handleSensorCommands(const String& cmd) {
    if (cmd == "STATUS_SENSORES" || cmd == "STATUS") {
        _sensors.printDetailedStatus();
        return true;
    }
    
    if (cmd == "RECALIBRAR_MAG" || cmd == "CALIB_MAG") {
        DEBUG_PRINTLN("[CommandHandler] Recalibrando magnetometro...");
        bool success = _sensors.recalibrateMagnetometer();
        
        if (success) {
            DEBUG_PRINTLN("[CommandHandler] Recalibracao concluida");
        } else {
            DEBUG_PRINTLN("[CommandHandler] Falha na recalibracao");
        }
        return true;
    }
    
    if (cmd == "LIMPAR_MAG" || cmd == "CLEAR_MAG") {
        DEBUG_PRINTLN("[CommandHandler] Limpando calibracao do magnetometro...");
        _sensors.clearMagnetometerCalibration();
        DEBUG_PRINTLN("[CommandHandler] Reinicie o sistema para recalibrar");
        return true;
    }
    
    if (cmd == "VER_MAG" || cmd == "INFO_MAG") {
        _sensors.printMagnetometerCalibration();
        return true;
    }
    
    return false;
}

bool CommandHandler::_handleCCS811Commands(const String& cmd) {
    if (cmd == "SALVAR_BASELINE" || cmd == "SAVE_BASELINE") {
        DEBUG_PRINTLN("[CommandHandler] Salvando baseline do CCS811...");
        
        if (_sensors.saveCCS811Baseline()) {
            DEBUG_PRINTLN("[CommandHandler] Baseline salvo com sucesso");
        } else {
            DEBUG_PRINTLN("[CommandHandler] Falha ao salvar baseline");
        }
        return true;
    }
    
    if (cmd == "RESTAURAR_BASELINE" || cmd == "RESTORE_BASELINE") {
        DEBUG_PRINTLN("[CommandHandler] Restaurando baseline do CCS811...");
        
        if (_sensors.restoreCCS811Baseline()) {
            DEBUG_PRINTLN("[CommandHandler] Baseline restaurado");
        } else {
            DEBUG_PRINTLN("[CommandHandler] Nenhum baseline salvo encontrado");
        }
        return true;
    }
    
    return false;
}

bool CommandHandler::_handleSystemCommands(const String& cmd) {
    if (cmd == "HELP" || cmd == "?") {
        _printHelpMenu();
        return true;
    }
    
    return false;
}

void CommandHandler::_printHelpMenu() {
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("COMANDOS DISPONIVEIS:");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("SENSORES:");
    DEBUG_PRINTLN("  STATUS_SENSORES   - Status detalhado de todos sensores");
    DEBUG_PRINTLN("  RECALIBRAR_MAG    - Recalibrar magnetometro MPU9250");
    DEBUG_PRINTLN("  LIMPAR_MAG        - Limpar calibracao salva");
    DEBUG_PRINTLN("  VER_MAG           - Ver calibracao atual");
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("CCS811 (Qualidade do Ar):");
    DEBUG_PRINTLN("  SALVAR_BASELINE   - Salvar baseline (apos 48h)");
    DEBUG_PRINTLN("  RESTAURAR_BASELINE- Restaurar baseline salvo");
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("SISTEMA:");
    DEBUG_PRINTLN("  HELP              - Mostrar este menu");
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("");
}
