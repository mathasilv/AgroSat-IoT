/**
 * @file StorageManager.cpp
 * @brief Implementação robusta do Storage com Hot-Swap
 */

#include "StorageManager.h"
#include "core/RTCManager/RTCManager.h"

// Instância global SPI para o SD (HSPI no ESP32)
SPIClass spiSD(HSPI);

StorageManager::StorageManager() :
    _available(false),
    _rtcManager(nullptr),
    _lastInitAttempt(0)
{
}

bool StorageManager::begin() {
    DEBUG_PRINTLN("[StorageManager] Inicializando SD Card...");
    
    // Configura SPI apenas uma vez
    spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS, spiSD)) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Falha ao montar SD Card.");
        _available = false;
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Nenhum cartão conectado.");
        _available = false;
        return false;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    DEBUG_PRINTF("[StorageManager] Cartão montado: %llu MB\n", cardSize);
    
    _available = true;
    
    // Garante que arquivos existem e escreve headers se necessário
    createTelemetryFile();
    createMissionFile();
    
    return true;
}

void StorageManager::setRTCManager(RTCManager* rtcManager) {
    _rtcManager = rtcManager;
}

// === Lógica de Recuperação (Hot-Swap) ===
void StorageManager::_attemptRecovery() {
    if (_available) return; // Já está online
    
    unsigned long now = millis();
    if (now - _lastInitAttempt < REINIT_INTERVAL) return; // Aguarda intervalo
    
    _lastInitAttempt = now;
    DEBUG_PRINTLN("[StorageManager] Tentando reconectar SD Card...");
    
    SD.end(); // Finaliza instância anterior
    if (begin()) {
        DEBUG_PRINTLN("[StorageManager] RECUPERADO COM SUCESSO!");
    }
}

// === Salvamento de Dados ===

bool StorageManager::saveTelemetry(const TelemetryData& data) {
    // Tenta recuperar se estiver offline
    if (!_available) {
        _attemptRecovery();
        if (!_available) return false;
    }
    
    // Verifica tamanho e rotaciona se necessário
    _checkFileSize(SD_LOG_FILE);
    
    File file = SD.open(SD_LOG_FILE, FILE_APPEND);
    if (!file) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Falha de escrita (Telemetria). SD removido?");
        _available = false; // Marca como falha para tentar recuperar na próxima
        return false;
    }
    
    file.println(_telemetryToCSV(data));
    file.close();
    
    return true;
}

bool StorageManager::saveMissionData(const MissionData& data) {
    if (!_available) {
        _attemptRecovery();
        if (!_available) return false;
    }
    
    _checkFileSize(SD_MISSION_FILE);
    
    File file = SD.open(SD_MISSION_FILE, FILE_APPEND);
    if (!file) {
        DEBUG_PRINTLN("[StorageManager] ERRO: Falha de escrita (Missão).");
        _available = false;
        return false;
    }
    
    file.println(_missionToCSV(data));
    file.close();
    
    return true;
}

bool StorageManager::logError(const String& errorMsg) {
    if (!_available) return false; // Erros não forçam recuperação para evitar loops
    
    File file = SD.open(SD_ERROR_FILE, FILE_APPEND);
    if (!file) return false;
    
    String logLine = "[" + _getTimestampStr() + "] " + errorMsg;
    file.println(logLine);
    file.close();
    
    return true;
}

// === Gerenciamento de Arquivos ===

bool StorageManager::createTelemetryFile() {
    if (SD.exists(SD_LOG_FILE)) return true;
    
    File file = SD.open(SD_LOG_FILE, FILE_WRITE);
    if (!file) return false;
    
    // Header rigorosamente alinhado com _telemetryToCSV
    file.print("ISO8601,UnixTimestamp,MissionTime,");
    file.print("BatVoltage,BatPercent,");
    file.print("TempFinal,TempBMP,TempSI,Pressure,Altitude,");
    file.print("GyroX,GyroY,GyroZ,AccelX,AccelY,AccelZ,MagX,MagY,MagZ,");
    file.print("Humidity,CO2,TVOC,");
    file.println("Status,Errors,Payload");
    
    file.close();
    return true;
}

bool StorageManager::createMissionFile() {
    if (SD.exists(SD_MISSION_FILE)) return true;
    
    File file = SD.open(SD_MISSION_FILE, FILE_WRITE);
    if (!file) return false;
    
    file.println("ISO8601,UnixTimestamp,NodeID,SoilMoisture,AmbTemp,Humidity,Irrigation,RSSI,SNR,PktsRx,PktsLost,LastRx");
    
    file.close();
    return true;
}

void StorageManager::listFiles() {
    if (!_available) return;
    
    DEBUG_PRINTLN("[StorageManager] --- Arquivos no SD ---");
    File root = SD.open("/");
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            DEBUG_PRINTF("  %s (%u bytes)\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
    DEBUG_PRINTLN("---------------------------");
}

uint64_t StorageManager::getFreeSpace() {
    if (!_available) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

uint64_t StorageManager::getUsedSpace() {
    if (!_available) return 0;
    return SD.usedBytes();
}

// === Helpers Privados ===

String StorageManager::_getTimestampStr() {
    if (_rtcManager && _rtcManager->isInitialized()) {
        return _rtcManager->getDateTime();
    }
    return String(millis());
}

bool StorageManager::_checkFileSize(const char* path) {
    if (!SD.exists(path)) return false;
    
    File file = SD.open(path, FILE_READ);
    size_t size = file.size();
    file.close();
    
    if (size > SD_MAX_FILE_SIZE) {
        String timestamp = _getTimestampStr();
        timestamp.replace(" ", "_");
        timestamp.replace(":", "-");
        
        String backup = String(path) + "." + timestamp + ".bak";
        SD.rename(path, backup.c_str());
        DEBUG_PRINTF("[StorageManager] Rotacionado: %s -> %s\n", path, backup.c_str());
        
        // Recria header
        if (strcmp(path, SD_LOG_FILE) == 0) createTelemetryFile();
        else createMissionFile();
    }
    return true;
}

String StorageManager::_telemetryToCSV(const TelemetryData& data) {
    char buffer[512];
    
    // Formatação segura de floats para evitar NAN quebrando o CSV
    auto safeF = [](float v) { return isnan(v) ? 0.0f : v; };
    
    // Helpers de string
    String ts = _getTimestampStr();
    String tempSI = isnan(data.temperatureSI) ? "" : String(data.temperatureSI, 2);
    String hum = isnan(data.humidity) ? "" : String(data.humidity, 1);
    
    snprintf(buffer, sizeof(buffer),
        "%s,%lu,%lu,"           // Time
        "%.2f,%.1f,"            // Power
        "%.2f,%.2f,%s,%.2f,%.1f," // Env (Final, BMP, SI, Press, Alt)
        "%.2f,%.2f,%.2f,"       // Gyro
        "%.2f,%.2f,%.2f,"       // Accel
        "%.1f,%.1f,%.1f,"       // Mag
        "%s,%.0f,%.0f,"         // Quality (Hum, CO2, TVOC)
        "0x%02X,%d,%s",         // Sys
        
        ts.c_str(), 
        (unsigned long)data.timestamp, 
        (unsigned long)data.missionTime,
        
        data.batteryVoltage, 
        data.batteryPercentage,
        
        safeF(data.temperature), 
        safeF(data.temperatureBMP), 
        tempSI.c_str(), 
        safeF(data.pressure), 
        safeF(data.altitude),
        
        safeF(data.gyroX), safeF(data.gyroY), safeF(data.gyroZ),
        safeF(data.accelX), safeF(data.accelY), safeF(data.accelZ),
        safeF(data.magX), safeF(data.magY), safeF(data.magZ),
        
        hum.c_str(), 
        safeF(data.co2), 
        safeF(data.tvoc),
        
        data.systemStatus, 
        data.errorCount, 
        data.payload
    );
    
    return String(buffer);
}

String StorageManager::_missionToCSV(const MissionData& data) {
    char buffer[256];
    String ts = _getTimestampStr();
    
    snprintf(buffer, sizeof(buffer),
        "%s,%lu,"
        "%u,%.1f,%.1f,%.1f,%d,"
        "%d,%.2f,%u,%u,%lu",
        
        ts.c_str(), (unsigned long)(_rtcManager ? _rtcManager->getUnixTime() : millis()/1000),
        data.nodeId, data.soilMoisture, data.ambientTemp, data.humidity, data.irrigationStatus,
        data.rssi, data.snr, data.packetsReceived, data.packetsLost, data.lastLoraRx
    );
    
    return String(buffer);
}