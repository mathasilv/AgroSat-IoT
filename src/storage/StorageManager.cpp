/**
 * @file StorageManager.cpp
 * @brief Implementação com Log de GPS Integrado
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
    Serial.println("[StorageManager] Inicializando SD Card...");
    
    spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS, spiSD)) {
        Serial.println("[StorageManager] ERRO: Falha ao montar SD Card.");
        _available = false;
        return false;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[StorageManager] ERRO: Nenhum cartão conectado.");
        _available = false;
        return false;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[StorageManager] Cartão montado: %llu MB\n", cardSize);
    
    _available = true;
    
    // Garante que arquivos existem e escreve headers se necessário
    createTelemetryFile();
    createMissionFile();
    createLogFile();
    
    return true;
}

void StorageManager::setRTCManager(RTCManager* rtcManager) {
    _rtcManager = rtcManager;
}

// === Lógica de Recuperação (Hot-Swap) ===

void StorageManager::_attemptRecovery() {
    if (_available) return; 
    
    unsigned long now = millis();
    if (now - _lastInitAttempt < REINIT_INTERVAL) return; 
    
    _lastInitAttempt = now;
    Serial.println("[StorageManager] Tentando reconectar SD Card...");
    
    SD.end(); 
    if (begin()) {
        Serial.println("[StorageManager] RECUPERADO COM SUCESSO!");
    }
}

// === Salvamento de Dados ===

bool StorageManager::saveTelemetry(const TelemetryData& data) {
    if (!_available) {
        _attemptRecovery();
        if (!_available) return false;
    }
    
    _checkFileSize(SD_LOG_FILE);
    
    File file = SD.open(SD_LOG_FILE, FILE_APPEND);
    if (!file) {
        _available = false; 
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
        _available = false;
        return false;
    }
    
    file.println(_missionToCSV(data));
    file.close();
    
    return true;
}

// === Log Geral do Sistema ===

bool StorageManager::saveLog(const String& message) {
    if (!_available) {
        _attemptRecovery();
        if (!_available) return false;
    }
    
    _checkFileSize(SD_SYSTEM_LOG);
    
    File file = SD.open(SD_SYSTEM_LOG, FILE_APPEND);
    if (!file) {
        _available = false;
        return false;
    }
    
    String logLine = "[" + _getTimestampStr() + "] " + message;
    file.println(logLine);
    file.close();
    
    return true;
}

bool StorageManager::logError(const String& errorMsg) {
    return saveLog("[ERROR] " + errorMsg);
}

// === Gerenciamento de Arquivos e Headers ===

bool StorageManager::createTelemetryFile() {
    // Se existe, não recria. (APAGUE O ARQUIVO ANTIGO SE QUISER O NOVO HEADER)
    if (SD.exists(SD_LOG_FILE)) return true;
    
    File file = SD.open(SD_LOG_FILE, FILE_WRITE);
    if (!file) return false;
    
    // [MODIFICADO] Header com colunas de GPS
    file.print("ISO8601,UnixTimestamp,MissionTime,BatVoltage,BatPercent,");
    file.print("TempFinal,TempBMP,TempSI,Pressure,Altitude,");
    file.print("Lat,Lng,GpsAlt,Sats,Fix,"); // <--- Colunas GPS
    file.print("GyroX,GyroY,GyroZ,AccelX,AccelY,AccelZ,MagX,MagY,MagZ,");
    file.print("Humidity,CO2,TVOC,Status,Errors,Payload"); 
    file.println();
    
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

bool StorageManager::createLogFile() {
    if (SD.exists(SD_SYSTEM_LOG)) return true;
    
    File file = SD.open(SD_SYSTEM_LOG, FILE_WRITE);
    if (!file) return false;
    
    file.println("=== AGROSAT-IOT SYSTEM LOG ===");
    file.println("Timestamp,Message");
    file.close();
    return true;
}

// === Utilitários de Arquivo ===

bool StorageManager::_checkFileSize(const char* path) {
    if (!SD.exists(path)) return false;
    
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    
    size_t size = file.size();
    file.close();
    
    if (size > SD_MAX_FILE_SIZE) { // 5MB
        String timestamp = _getTimestampStr();
        timestamp.replace(" ", "_");
        timestamp.replace(":", "-");
        
        String backupPath = String(path) + "." + timestamp + ".bak";
        
        SD.rename(path, backupPath.c_str());
        Serial.printf("[StorageManager] Arquivo rotacionado: %s -> %s\n", path, backupPath.c_str());
        
        if (strcmp(path, SD_LOG_FILE) == 0) createTelemetryFile();
        else if (strcmp(path, SD_MISSION_FILE) == 0) createMissionFile();
        else if (strcmp(path, SD_SYSTEM_LOG) == 0) createLogFile();
    }
    
    return true;
}

void StorageManager::listFiles() {
    if (!_available) return;
    
    Serial.println("[StorageManager] --- Arquivos no SD ---");
    File root = SD.open("/");
    File file = root.openNextFile();
    
    while (file) {
        if (!file.isDirectory()) {
            Serial.printf("  %s (%u bytes)\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
    Serial.println("---------------------------");
}

uint64_t StorageManager::getFreeSpace() {
    if (!_available) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

uint64_t StorageManager::getUsedSpace() {
    if (!_available) return 0;
    return SD.usedBytes();
}

// === Helpers de Formatação ===

String StorageManager::_getTimestampStr() {
    if (_rtcManager && _rtcManager->isInitialized()) {
        return _rtcManager->getDateTime();
    }
    return String(millis());
}

String StorageManager::_telemetryToCSV(const TelemetryData& data) {
    char buffer[600];
    
    auto safeF = [](float v) { return isnan(v) ? 0.0f : v; };
    
    String ts = _getTimestampStr();
    String tempSI = isnan(data.temperatureSI) ? "" : String(data.temperatureSI, 2);
    String hum = isnan(data.humidity) ? "" : String(data.humidity, 1);
    
    // [MODIFICADO] Inserção dos dados de GPS
    // Lat/Lng com 6 casas decimais de precisão
    snprintf(buffer, sizeof(buffer),
        "%s,%lu,%lu,"             // Time
        "%.2f,%.1f,"              // Power
        "%.2f,%.2f,%s,%.2f,%.1f," // Env
        "%.6f,%.6f,%.1f,%d,%d,"   // GPS (Lat, Lng, Alt, Sats, Fix)
        "%.2f,%.2f,%.2f,"         // Gyro
        "%.2f,%.2f,%.2f,"         // Accel
        "%.1f,%.1f,%.1f,"         // Mag
        "%s,%.0f,%.0f,"           // Air Quality
        "0x%02X,%d,%s",           // System
        
        ts.c_str(), (unsigned long)data.timestamp, (unsigned long)data.missionTime,
        data.batteryVoltage, data.batteryPercentage,
        
        safeF(data.temperature), safeF(data.temperatureBMP), tempSI.c_str(), safeF(data.pressure), safeF(data.altitude),
        
        // Dados GPS
        data.latitude, data.longitude, safeF(data.gpsAltitude), data.satellites, data.gpsFix,
        
        safeF(data.gyroX), safeF(data.gyroY), safeF(data.gyroZ),
        safeF(data.accelX), safeF(data.accelY), safeF(data.accelZ),
        safeF(data.magX), safeF(data.magY), safeF(data.magZ),
        
        hum.c_str(), safeF(data.co2), safeF(data.tvoc),
        data.systemStatus, data.errorCount, data.payload
    );
    
    return String(buffer);
}

String StorageManager::_missionToCSV(const MissionData& data) {
    char buffer[256];
    String ts = _getTimestampStr();
    
    snprintf(buffer, sizeof(buffer), 
        "%s,%lu,"      
        "%u,"          
        "%.1f,%.1f,%.1f,%d," 
        "%d,%.2f,"     
        "%u,%u,%lu",   
        
        ts.c_str(), (unsigned long)(_rtcManager ? _rtcManager->getUnixTime() : millis()/1000),
        data.nodeId, 
        data.soilMoisture, data.ambientTemp, data.humidity, data.irrigationStatus,
        data.rssi, data.snr, 
        data.packetsReceived, data.packetsLost, data.lastLoraRx
    );
    
    return String(buffer);
}