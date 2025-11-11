/**
 * @file StorageManager.cpp
 * @brief Implementação do gerenciador de armazenamento - COM RTC
 * @version 1.1.0
 * @date 2025-11-10
 */

#include "StorageManager.h"
#include "RTCManager.h"  // ✅ NOVO

SPIClass spiSD(HSPI);

StorageManager::StorageManager() :
    _available(false),
    _rtcManager(nullptr)  // ✅ NOVO
{
}

bool StorageManager::begin() {
    DEBUG_PRINTLN("[StorageManager] Inicializando SD Card...");
    
    // Configurar SPI para SD Card
    spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    
    // Inicializar SD Card
    if (!SD.begin(SD_CS, spiSD)) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Falha ao inicializar SD Card!");
        _available = false;
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    
    if (cardType == CARD_NONE) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Nenhum cartão SD detectado!");
        _available = false;
        return false;
    }
    
    // Exibir informações do SD Card
    DEBUG_PRINT("[StorageManager] Tipo de cartão: ");
    if (cardType == CARD_MMC) {
        DEBUG_PRINTLN("MMC");
    } else if (cardType == CARD_SD) {
        DEBUG_PRINTLN("SDSC");
    } else if (cardType == CARD_SDHC) {
        DEBUG_PRINTLN("SDHC");
    } else {
        DEBUG_PRINTLN("UNKNOWN");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    DEBUG_PRINTF("[StorageManager] Tamanho do cartão: %llu MB\n", cardSize);
    
    _available = true;
    
    // Criar arquivos se não existirem
    createTelemetryFile();
    createMissionFile();
    
    return true;
}

bool StorageManager::saveTelemetry(const TelemetryData& data) {
    if (!_available) return false;
    
    // Verificar tamanho do arquivo
    _checkFileSize(SD_LOG_FILE);
    
    // Abrir arquivo
    File file = SD.open(SD_LOG_FILE, FILE_APPEND);
    if (!file) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Não foi possível abrir arquivo de telemetria");
        return false;
    }
    
    // Escrever linha CSV
    String csvLine = _telemetryToCSV(data);
    file.println(csvLine);
    
    file.close();
    
    return true;
}

bool StorageManager::saveMissionData(const MissionData& data) {
    if (!_available) return false;
    
    // Verificar tamanho do arquivo
    _checkFileSize(SD_MISSION_FILE);
    
    // Abrir arquivo
    File file = SD.open(SD_MISSION_FILE, FILE_APPEND);
    if (!file) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Não foi possível abrir arquivo de missão");
        return false;
    }
    
    // Escrever linha CSV
    String csvLine = _missionToCSV(data);
    file.println(csvLine);
    
    file.close();
    
    return true;
}

// ============================================================================
// ✅ MÉTODO ATUALIZADO COM RTC
// ============================================================================
bool StorageManager::logError(const String& errorMsg) {
    if (!_available) return false;
    
    File file = SD.open(SD_ERROR_FILE, FILE_APPEND);
    if (!file) return false;
    
    String timestamp;
    
    // ✅ USAR TIMESTAMP ISO8601 DO RTC SE DISPONÍVEL
    if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
        timestamp = _rtcManager->getISO8601();
    } else {
        // Fallback: usar millis()
        timestamp = String(millis());
    }
    
    String logLine = "[" + timestamp + "] " + errorMsg;
    file.println(logLine);
    
    file.close();
    
    DEBUG_PRINTF("[StorageManager] Erro registrado: %s\n", errorMsg.c_str());
    
    return true;
}

bool StorageManager::createTelemetryFile() {
    if (!_available) return false;
    
    // Verificar se arquivo já existe
    if (SD.exists(SD_LOG_FILE)) {
        DEBUG_PRINTLN("[StorageManager] Arquivo de telemetria já existe");
        return true;
    }
    
    // Criar arquivo com header CSV
    File file = SD.open(SD_LOG_FILE, FILE_WRITE);
    if (!file) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Não foi possível criar arquivo de telemetria");
        return false;
    }
    
    // ✅ HEADER CSV ATUALIZADO COM TIMESTAMP REAL
    file.println("ISO8601,UnixTimestamp,MissionTime,BatteryVoltage,BatteryPercentage,Temperature,Pressure,Altitude,GyroX,GyroY,GyroZ,AccelX,AccelY,AccelZ,Humidity,CO2,TVOC,MagX,MagY,MagZ,Status,Errors,Payload");
    
    file.close();
    
    DEBUG_PRINTLN("[StorageManager] Arquivo de telemetria criado");
    return true;
}

bool StorageManager::createMissionFile() {
    if (!_available) return false;
    
    // Verificar se arquivo já existe
    if (SD.exists(SD_MISSION_FILE)) {
        DEBUG_PRINTLN("[StorageManager] Arquivo de missão já existe");
        return true;
    }
    
    // Criar arquivo com header CSV
    File file = SD.open(SD_MISSION_FILE, FILE_WRITE);
    if (!file) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Não foi possível criar arquivo de missão");
        return false;
    }
    
    // ✅ HEADER CSV ATUALIZADO
    file.println("ISO8601,UnixTimestamp,SoilMoisture,AmbientTemp,Humidity,IrrigationStatus,RSSI,SNR,PacketsReceived,PacketsLost,LastRx");
    
    file.close();
    
    DEBUG_PRINTLN("[StorageManager] Arquivo de missão criado");
    return true;
}

bool StorageManager::isAvailable() {
    return _available;
}

uint64_t StorageManager::getFreeSpace() {
    if (!_available) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

uint64_t StorageManager::getUsedSpace() {
    if (!_available) return 0;
    return SD.usedBytes();
}

void StorageManager::listFiles() {
    if (!_available) return;
    
    DEBUG_PRINTLN("[StorageManager] Arquivos no SD Card:");
    
    File root = SD.open("/");
    File file = root.openNextFile();
    
    while (file) {
        DEBUG_PRINTF("  %s - %d bytes\n", file.name(), file.size());
        file = root.openNextFile();
    }
}

void StorageManager::flush() {
    // Força escrita no SD (não implementado diretamente no ESP32)
    // Dados são escritos ao fechar arquivo
}

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

File StorageManager::_openFile(const char* path) {
    return SD.open(path, FILE_APPEND);
}

// ============================================================================
// ✅ MÉTODO ATUALIZADO COM RTC
// ============================================================================
bool StorageManager::_checkFileSize(const char* path) {
    if (!SD.exists(path)) return false;
    
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    
    size_t fileSize = file.size();
    file.close();
    
    // Verificar se excedeu tamanho máximo
    if (fileSize > SD_MAX_FILE_SIZE) {
        DEBUG_PRINTF("[StorageManager] Arquivo %s excedeu tamanho máximo. Rotacionando...\n", path);
        
        String backupPath;
        
        // ✅ USAR TIMESTAMP DO RTC NO NOME DO BACKUP
        if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
            String timestamp = _rtcManager->getDateString(); // "2025-11-10"
            backupPath = String(path) + "." + timestamp + ".bak";
        } else {
            // Fallback: usar millis()
            backupPath = String(path) + "." + String(millis()) + ".bak";
        }
        
        // Remover backup antigo se existir
        if (SD.exists(backupPath.c_str())) {
            SD.remove(backupPath.c_str());
        }
        
        // Renomear arquivo atual para backup
        SD.rename(path, backupPath.c_str());
        
        DEBUG_PRINTF("[StorageManager] Arquivo rotacionado para: %s\n", backupPath.c_str());
        
        // Criar novo arquivo
        if (strcmp(path, SD_LOG_FILE) == 0) {
            createTelemetryFile();
        } else if (strcmp(path, SD_MISSION_FILE) == 0) {
            createMissionFile();
        }
    }
    
    return true;
}

// ============================================================================
// ✅ MÉTODO ATUALIZADO COM TIMESTAMP RTC
// ============================================================================
String StorageManager::_telemetryToCSV(const TelemetryData& data) {
    String csv = "";
    
    // ✅ ADICIONAR TIMESTAMP ISO8601 SE RTC DISPONÍVEL
    if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
        csv += _rtcManager->getISO8601() + ",";
    } else {
        csv += "N/A,";
    }
    
    csv += String(data.timestamp) + ",";
    csv += String(data.missionTime) + ",";
    csv += String(data.batteryVoltage, 2) + ",";
    csv += String(data.batteryPercentage, 1) + ",";
    csv += String(data.temperature, 2) + ",";
    csv += String(data.pressure, 2) + ",";
    csv += String(data.altitude, 1) + ",";
    csv += String(data.gyroX, 4) + ",";
    csv += String(data.gyroY, 4) + ",";
    csv += String(data.gyroZ, 4) + ",";
    csv += String(data.accelX, 4) + ",";
    csv += String(data.accelY, 4) + ",";
    csv += String(data.accelZ, 4) + ",";
    
    // Campos opcionais
    csv += isnan(data.humidity) ? "," : String(data.humidity, 2) + ",";
    csv += isnan(data.co2) ? "," : String(data.co2, 0) + ",";
    csv += isnan(data.tvoc) ? "," : String(data.tvoc, 0) + ",";
    csv += isnan(data.magX) ? "," : String(data.magX, 2) + ",";
    csv += isnan(data.magY) ? "," : String(data.magY, 2) + ",";
    csv += isnan(data.magZ) ? "," : String(data.magZ, 2) + ",";
    
    csv += String(data.systemStatus) + ",";
    csv += String(data.errorCount) + ",";
    csv += String(data.payload);
    
    return csv;
}

// ============================================================================
// ✅ MÉTODO ATUALIZADO COM TIMESTAMP RTC
// ============================================================================
String StorageManager::_missionToCSV(const MissionData& data) {
    String csv = "";
    
    // ✅ ADICIONAR TIMESTAMP ISO8601 SE RTC DISPONÍVEL
    if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
        csv += _rtcManager->getISO8601() + ",";
    } else {
        csv += "N/A,";
    }
    
    csv += String(millis()) + ",";
    csv += String(data.soilMoisture, 2) + ",";
    csv += String(data.ambientTemp, 2) + ",";
    csv += String(data.humidity, 2) + ",";
    csv += String(data.irrigationStatus) + ",";
    csv += String(data.rssi) + ",";
    csv += String(data.snr, 2) + ",";
    csv += String(data.packetsReceived) + ",";
    csv += String(data.packetsLost) + ",";
    csv += String(data.lastLoraRx);
    
    return csv;
}
