/**
 * @file StorageManager.cpp
 * @brief Implementação com Zero Fragmentação de Heap
 */

#include "StorageManager.h"
#include "core/RTCManager/RTCManager.h"

// Instância global SPI para o SD (HSPI no ESP32)
SPIClass spiSD(HSPI);

// Buffer estático para formatação (evita alocação dinâmica repetitiva)
static char fmtBuffer[512];

StorageManager::StorageManager() :
    _available(false),
    _rtcManager(nullptr),
    _lastInitAttempt(0)
{
}

bool StorageManager::begin() {
    Serial.println("[StorageManager] Inicializando SD Card...");
    
    spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    
    // Aumentamos a frequência para 4MHz para garantir estabilidade
    if (!SD.begin(SD_CS, spiSD, 4000000)) {
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
    
    // Escrita otimizada sem String()
    _formatTelemetryToCSV(data, fmtBuffer, sizeof(fmtBuffer));
    file.println(fmtBuffer);
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
    
    _formatMissionToCSV(data, fmtBuffer, sizeof(fmtBuffer));
    file.println(fmtBuffer);
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
    
    // Usa o buffer estático para formatar o log
    String ts;
    if (_rtcManager && _rtcManager->isInitialized()) {
        ts = _rtcManager->getDateTime();
    } else {
        ts = String(millis());
    }
    
    snprintf(fmtBuffer, sizeof(fmtBuffer), "[%s] %s", ts.c_str(), message.c_str());
    file.println(fmtBuffer);
    file.close();
    
    return true;
}

bool StorageManager::logError(const String& errorMsg) {
    return saveLog("[ERROR] " + errorMsg);
}

// === Gerenciamento de Arquivos e Headers ===

bool StorageManager::createTelemetryFile() {
    if (SD.exists(SD_LOG_FILE)) return true;
    
    File file = SD.open(SD_LOG_FILE, FILE_WRITE);
    if (!file) return false;
    
    // Header otimizado com F-macro para economizar RAM
    file.print(F("ISO8601,UnixTimestamp,MissionTime,BatVoltage,BatPercent,"));
    file.print(F("TempFinal,TempBMP,TempSI,Pressure,Altitude,"));
    file.print(F("Lat,Lng,GpsAlt,Sats,Fix,")); 
    file.print(F("GyroX,GyroY,GyroZ,AccelX,AccelY,AccelZ,MagX,MagY,MagZ,"));
    file.println(F("Humidity,CO2,TVOC,Status,Errors,Payload")); 
    
    file.close();
    return true;
}

bool StorageManager::createMissionFile() {
    if (SD.exists(SD_MISSION_FILE)) return true;
    
    File file = SD.open(SD_MISSION_FILE, FILE_WRITE);
    if (!file) return false;
    
    file.println(F("ISO8601,UnixTimestamp,NodeID,SoilMoisture,AmbTemp,Humidity,Irrigation,RSSI,SNR,PktsRx,PktsLost,LastRx"));
    
    file.close();
    return true;
}

bool StorageManager::createLogFile() {
    if (SD.exists(SD_SYSTEM_LOG)) return true;
    
    File file = SD.open(SD_SYSTEM_LOG, FILE_WRITE);
    if (!file) return false;
    
    file.println(F("=== AGROSAT-IOT SYSTEM LOG ==="));
    file.println(F("Timestamp,Message"));
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
    
    if (size > SD_MAX_FILE_SIZE) { 
        String timestamp;
        if (_rtcManager && _rtcManager->isInitialized()) {
             timestamp = _rtcManager->getDateTime();
        } else {
             timestamp = String(millis());
        }
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
    
    Serial.println(F("[StorageManager] --- Arquivos no SD ---"));
    File root = SD.open("/");
    File file = root.openNextFile();
    
    while (file) {
        if (!file.isDirectory()) {
            Serial.printf("  %s (%u bytes)\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
    Serial.println(F("---------------------------"));
}

uint64_t StorageManager::getFreeSpace() {
    if (!_available) return 0;
    return SD.totalBytes() - SD.usedBytes();
}

uint64_t StorageManager::getUsedSpace() {
    if (!_available) return 0;
    return SD.usedBytes();
}

// === Helpers de Formatação (Implementação Otimizada) ===

void StorageManager::_formatTelemetryToCSV(const TelemetryData& data, char* buffer, size_t len) {
    auto safeF = [](float v) { return isnan(v) ? 0.0f : v; };

    String ts = (_rtcManager && _rtcManager->isInitialized()) ? 
                _rtcManager->getDateTime() : String(millis());

    snprintf(buffer, len,
        "%s,%lu,%lu,"             // Time
        "%.2f,%.1f,"              // Power
        "%.2f,%.2f,%.2f,%.2f,%.1f," // Env
        "%.6f,%.6f,%.1f,%d,%d,"   // GPS
        "%.2f,%.2f,%.2f,"         // Gyro
        "%.2f,%.2f,%.2f,"         // Accel
        "%.1f,%.1f,%.1f,"         // Mag
        "%.1f,%.0f,%.0f,"         // Air Quality
        "0x%02X,%d,%s",           // System
        
        ts.c_str(), (unsigned long)data.timestamp, (unsigned long)data.missionTime,
        data.batteryVoltage, data.batteryPercentage,
        
        safeF(data.temperature), safeF(data.temperatureBMP), safeF(data.temperatureSI), safeF(data.pressure), safeF(data.altitude),
        
        data.latitude, data.longitude, safeF(data.gpsAltitude), data.satellites, data.gpsFix,
        
        safeF(data.gyroX), safeF(data.gyroY), safeF(data.gyroZ),
        safeF(data.accelX), safeF(data.accelY), safeF(data.accelZ),
        safeF(data.magX), safeF(data.magY), safeF(data.magZ),
        
        safeF(data.humidity), safeF(data.co2), safeF(data.tvoc),
        data.systemStatus, data.errorCount, data.payload
    );
}

void StorageManager::_formatMissionToCSV(const MissionData& data, char* buffer, size_t len) {
    String ts = (_rtcManager && _rtcManager->isInitialized()) ? 
                _rtcManager->getDateTime() : String(millis());
    
    snprintf(buffer, len, 
        "%s,%lu,"      
        "%u,"          
        "%.1f,%.1f,%.1f,%d," 
        "%d,%.2f,"     
        "%u,%u,%lu",   
        
        ts.c_str(), 
        (_rtcManager && _rtcManager->isInitialized()) ? _rtcManager->getUnixTime() : (unsigned long)(millis()/1000),
        data.nodeId, 
        data.soilMoisture, data.ambientTemp, data.humidity, data.irrigationStatus,
        data.rssi, data.snr, 
        data.packetsReceived, data.packetsLost, data.lastLoraRx
    );
}