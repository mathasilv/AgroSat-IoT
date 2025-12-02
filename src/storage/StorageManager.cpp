/**
 * @file StorageManager.cpp
 * @brief Implementação do gerenciador de armazenamento - COM RTC
 * @version 1.2.0
 * @date 2025-11-12
 * 
 * CHANGELOG v1.2.0:
 * - Corrigido header CSV com ISO8601 timestamp
 * - Removido uso de métodos inexistentes do RTCManager
 * - Padronizado uso de getDateTime() e getUnixTime()
 * - Corrigido buffer overflow em _telemetryToCSV
 * - Adicionada injeção de dependência do RTCManager
 */

#include "StorageManager.h"
#include "core/RTCManager/RTCManager.h"

SPIClass spiSD(HSPI);

StorageManager::StorageManager() :
    _available(false),
    _rtcManager(nullptr)
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

// ✅ NOVO: Injeção de dependência do RTCManager
void StorageManager::setRTCManager(RTCManager* rtcManager) {
    _rtcManager = rtcManager;
    DEBUG_PRINTLN("[StorageManager] RTCManager vinculado");
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

// ✅ CORRIGIDO: Apenas usa getDateTime()
bool StorageManager::logError(const String& errorMsg) {
    if (!_available) return false;
    
    File file = SD.open(SD_ERROR_FILE, FILE_APPEND);
    if (!file) return false;
    
    String timestamp;
    
    if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
        timestamp = _rtcManager->getDateTime();
    } else {
        timestamp = String(millis());
    }
    
    String logLine = "[" + timestamp + "] " + errorMsg;
    file.println(logLine);
    
    file.close();
    
    DEBUG_PRINTF("[StorageManager] Erro registrado: %s\n", errorMsg.c_str());
    
    return true;
}

// ✅ CORRIGIDO: Header CSV com ISO8601
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
    
    // ✅ HEADER CSV COMPLETO COM ISO8601
    file.print("ISO8601,");              // ✅ ADICIONADO
    file.print("UnixTimestamp,");
    file.print("MissionTime,");
    file.print("BatteryVoltage,");
    file.print("BatteryPercentage,");
    file.print("TempBMP280,");
    file.print("TempSI7021,");
    file.print("TempDelta,");
    file.print("Pressure,");
    file.print("Altitude,");
    file.print("GyroX,");
    file.print("GyroY,");
    file.print("GyroZ,");
    file.print("AccelX,");
    file.print("AccelY,");
    file.print("AccelZ,");
    file.print("Humidity,");
    file.print("CO2,");
    file.print("TVOC,");
    file.print("MagX,");
    file.print("MagY,");
    file.print("MagZ,");
    file.print("Status,");
    file.print("Errors,");
    file.println("Payload");
    
    file.close();
    
    DEBUG_PRINTLN("[StorageManager] Arquivo de telemetria criado com sucesso");
    
    return true;
}

// ✅ CORRIGIDO: Header CSV com ISO8601
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
    
    // ✅ HEADER COM ISO8601
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

// ============================================================================
// MÉTODOS PRIVADOS
// ============================================================================

File StorageManager::_openFile(const char* path) {
    return SD.open(path, FILE_APPEND);
}

// ✅ CORRIGIDO: Usa getDateTime() para backup
bool StorageManager::_checkFileSize(const char* path) {
    if (!SD.exists(path)) return false;
    
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    
    size_t fileSize = file.size();
    file.close();
    
    if (fileSize > SD_MAX_FILE_SIZE) {
        DEBUG_PRINTF("[StorageManager] Arquivo %s excedeu tamanho máximo. Rotacionando...\n", path);
        
        String backupPath;
        
        if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
            // Extrair apenas a data (primeiros 10 caracteres de "YYYY-MM-DD HH:MM:SS")
            String dateTime = _rtcManager->getDateTime();
            String dateOnly = dateTime.substring(0, 10);  // "YYYY-MM-DD"
            dateOnly.replace("-", "");                     // "YYYYMMDD"
            
            backupPath = String(path) + "." + dateOnly + ".bak";
        } else {
            backupPath = String(path) + "." + String(millis()) + ".bak";
        }
        
        if (SD.exists(backupPath.c_str())) {
            SD.remove(backupPath.c_str());
        }
        
        SD.rename(path, backupPath.c_str());
        
        DEBUG_PRINTF("[StorageManager] Arquivo rotacionado para: %s\n", backupPath.c_str());
        
        // Recriar arquivo com header
        if (strcmp(path, SD_LOG_FILE) == 0) {
            createTelemetryFile();
        } else if (strcmp(path, SD_MISSION_FILE) == 0) {
            createMissionFile();
        }
    }
    
    return true;
}

// ✅ CORRIGIDO: Buffer expandido e formatação correta
String StorageManager::_telemetryToCSV(const TelemetryData& data) {
    char csvBuffer[600];  // ✅ Aumentado de 512 para 600 bytes
    
    // ========================================
    // TIMESTAMP ISO8601
    // ========================================
    String iso8601 = "N/A";
    if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
        iso8601 = _rtcManager->getDateTime();
    }
    
    // ========================================
    // TEMPERATURA DELTA
    // ========================================
    float tempDelta = 0.0;
    if (!isnan(data.temperature) && !isnan(data.temperatureSI)) {
        tempDelta = data.temperature - data.temperatureSI;
    }
    
    // ========================================
    // PREPARAR STRINGS TEMPORÁRIAS
    // ========================================
    String tempSI = isnan(data.temperatureSI) ? "" : String(data.temperatureSI, 2);
    String delta = (isnan(data.temperature) || isnan(data.temperatureSI)) ? "" : String(tempDelta, 2);
    String hum = isnan(data.humidity) ? "" : String(data.humidity, 2);
    String co2 = isnan(data.co2) ? "" : String(data.co2, 0);
    String tvoc = isnan(data.tvoc) ? "" : String(data.tvoc, 0);
    String magX = isnan(data.magX) ? "" : String(data.magX, 2);
    String magY = isnan(data.magY) ? "" : String(data.magY, 2);
    String magZ = isnan(data.magZ) ? "" : String(data.magZ, 2);
    
    // ========================================
    // FORMATAR CSV COMPLETO
    // ========================================
    snprintf(csvBuffer, sizeof(csvBuffer),
        "%s,"           // ISO8601 timestamp
        "%lu,"          // Unix timestamp
        "%lu,"          // Mission time
        "%.2f,"         // Battery voltage
        "%.1f,"         // Battery percentage
        "%.2f,"         // Temperature BMP280
        "%s,"           // Temperature SI7021
        "%s,"           // Temperature delta
        "%.2f,"         // Pressure
        "%.1f,"         // Altitude
        "%.4f,"         // GyroX
        "%.4f,"         // GyroY
        "%.4f,"         // GyroZ
        "%.4f,"         // AccelX
        "%.4f,"         // AccelY
        "%.4f,"         // AccelZ
        "%s,"           // Humidity
        "%s,"           // CO2
        "%s,"           // TVOC
        "%s,"           // MagX
        "%s,"           // MagY
        "%s,"           // MagZ
        "%d,"           // System status
        "%d,"           // Error count
        "%s",           // Payload
        
        // ✅ VALORES
        iso8601.c_str(),
        (unsigned long)data.timestamp,
        (unsigned long)data.missionTime,
        data.batteryVoltage,
        data.batteryPercentage,
        data.temperature,
        tempSI.c_str(),
        delta.c_str(),
        data.pressure,
        data.altitude,
        data.gyroX,
        data.gyroY,
        data.gyroZ,
        data.accelX,
        data.accelY,
        data.accelZ,
        hum.c_str(),
        co2.c_str(),
        tvoc.c_str(),
        magX.c_str(),
        magY.c_str(),
        magZ.c_str(),
        data.systemStatus,
        data.errorCount,
        data.payload
    );
    
    return String(csvBuffer);
}

String StorageManager::_missionToCSV(const MissionData& data) {
    char csvBuffer[256];
    
    String iso8601 = "N/A";
    uint32_t unixTime = millis();
    
    if (_rtcManager != nullptr && _rtcManager->isInitialized()) {
        iso8601 = _rtcManager->getDateTime();
        unixTime = _rtcManager->getUnixTime();
    }
    
    snprintf(csvBuffer, sizeof(csvBuffer),
        "%s,"       // ISO8601
        "%lu,"      // UnixTimestamp
        "%.2f,"     // SoilMoisture
        "%.2f,"     // AmbientTemp
        "%.2f,"     // Humidity
        "%d,"       // IrrigationStatus
        "%d,"       // RSSI
        "%.2f,"     // SNR
        "%d,"       // PacketsReceived
        "%d,"       // PacketsLost
        "%lu",      // LastRx
        
        iso8601.c_str(),
        unixTime,
        data.soilMoisture,
        data.ambientTemp,
        data.humidity,
        data.irrigationStatus,
        data.rssi,
        data.snr,
        data.packetsReceived,
        data.packetsLost,
        data.lastLoraRx
    );
    
    return String(csvBuffer);
}
