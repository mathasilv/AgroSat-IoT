/**
 * @file StorageManager.cpp
 * @brief Gerenciador de Armazenamento (FIX: Buffer local para thread-safety)
 * @version 3.4.0
 */

#include "StorageManager.h"
#include "core/RTCManager/RTCManager.h"
#include "core/SystemHealth/SystemHealth.h"

SPIClass spiSD(HSPI);

StorageManager::StorageManager() : 
    _available(false), 
    _rtcManager(nullptr), 
    _systemHealth(nullptr),
    _lastInitAttempt(0),
    _crcErrors(0),
    _totalWrites(0) 
{}

bool StorageManager::begin() {
    Serial.println("[StorageManager] Inicializando SD Card...");
    
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
    delay(10); 

    spiSD.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS, spiSD, 4000000)) {
        Serial.println("[StorageManager] ERRO: Falha ao montar SD Card.");
        _available = false; 
        return false;
    }
    
    if (SD.cardType() == CARD_NONE) {
        Serial.println("[StorageManager] ERRO: Nenhum cartão detectado.");
        _available = false; 
        return false;
    }
    
    _available = true;
    createTelemetryFile();
    createMissionFile();
    createLogFile();
    
    Serial.println("[StorageManager] SD Card inicializado com sucesso!");
    return true;
}

void StorageManager::setRTCManager(RTCManager* rtcManager) { 
    _rtcManager = rtcManager; 
}

void StorageManager::setSystemHealth(SystemHealth* systemHealth) {
    _systemHealth = systemHealth;
}

void StorageManager::_attemptRecovery() {
    unsigned long now = millis();
    if (now - _lastInitAttempt < REINIT_INTERVAL) return;
    _lastInitAttempt = now;
    
    Serial.println("[StorageManager] Tentando recuperar SD Card (Hard Reset)...");
    
    SD.end();
    spiSD.end(); 
    delay(100);
    
    if (begin()) {
        Serial.println("[StorageManager] RECUPERADO COM SUCESSO!");
        saveLog("Sistema de arquivos recuperado apos falha/remocao.");
    } else {
        Serial.println("[StorageManager] Recuperação falhou. Tentarei novamente em breve.");
    }
}

bool StorageManager::saveTelemetry(const TelemetryData& data) {
    if (!_available) { 
        _attemptRecovery(); 
        if (!_available) return false; 
    }
    
    _checkFileSize(SD_LOG_FILE);
    File file = SD.open(SD_LOG_FILE, FILE_APPEND);
    if (!file) { 
        Serial.println("[StorageManager] Erro de escrita (Telemetry). Marcando como indisponível.");
        _available = false; 
        return false; 
    }
    
    // FIX: Buffer local para thread-safety
    char localBuffer[512];
    _formatTelemetryToCSV(data, localBuffer, sizeof(localBuffer));
    
    uint16_t crc = _calculateCRC16((uint8_t*)localBuffer, strlen(localBuffer));
    char lineWithCRC[600];
    snprintf(lineWithCRC, sizeof(lineWithCRC), "%s,%04X", localBuffer, crc);
    
    file.println(lineWithCRC);
    file.close();
    
    _totalWrites++;
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
        Serial.println("[StorageManager] Erro de escrita (Mission). Marcando como indisponível.");
        _available = false; 
        return false; 
    }
    
    // FIX: Buffer local para thread-safety
    char localBuffer[512];
    _formatMissionToCSV(data, localBuffer, sizeof(localBuffer));
    
    uint16_t crc = _calculateCRC16((uint8_t*)localBuffer, strlen(localBuffer));
    char lineWithCRC[400];
    snprintf(lineWithCRC, sizeof(lineWithCRC), "%s,%04X", localBuffer, crc);
    
    file.println(lineWithCRC);
    file.close();
    
    _totalWrites++;
    return true;
}

uint16_t StorageManager::_calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

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
    
    String ts = (_rtcManager && _rtcManager->isInitialized()) ? 
                _rtcManager->getDateTime() : String(millis());
    
    // FIX: Buffer local para thread-safety
    char localBuffer[512];
    snprintf(localBuffer, sizeof(localBuffer), "[%s] %s", ts.c_str(), message.c_str());
    
    uint16_t crc = _calculateCRC16((uint8_t*)localBuffer, strlen(localBuffer));
    char lineWithCRC[600];
    snprintf(lineWithCRC, sizeof(lineWithCRC), "%s,%04X", localBuffer, crc);
    
    file.println(lineWithCRC);
    file.close();
    return true;
}

bool StorageManager::createTelemetryFile() {
    if (SD.exists(SD_LOG_FILE)) return true;
    File file = SD.open(SD_LOG_FILE, FILE_WRITE);
    if (!file) return false;
    
    file.print(F("ISO8601,UnixTimestamp,MissionTime,BatVoltage,BatPercent,"));
    file.print(F("TempFinal,TempBMP,TempSI,Pressure,Altitude,"));
    file.print(F("Lat,Lng,GpsAlt,Sats,Fix,"));
    file.print(F("GyroX,GyroY,GyroZ,AccelX,AccelY,AccelZ,MagX,MagY,MagZ,"));
    file.print(F("Humidity,CO2,TVOC,Status,Errors,Payload,"));
    file.println(F("Uptime,ResetCnt,MinHeap,CpuTemp,CRC16"));
    file.close();
    return true;
}

bool StorageManager::createMissionFile() {
    if (SD.exists(SD_MISSION_FILE)) return true;
    File file = SD.open(SD_MISSION_FILE, FILE_WRITE);
    if (!file) return false;
    
    file.print(F("ISO8601,UnixTimestamp,NodeID,SoilMoisture,AmbTemp,Humidity,"));
    file.print(F("Irrigation,RSSI,SNR,PktsRx,PktsLost,LastRx,"));
    file.println(F("NodeOriginTS,SatArrivalTS,SatTxTS,CRC16"));
    file.close();
    return true;
}

bool StorageManager::createLogFile() {
    if (SD.exists(SD_SYSTEM_LOG)) return true;
    File file = SD.open(SD_SYSTEM_LOG, FILE_WRITE);
    if (!file) return false;
    
    file.println(F("=== AGROSAT-IOT SYSTEM LOG v3.4 ==="));
    file.println(F("Timestamp,Message,CRC16"));
    file.close();
    return true;
}

bool StorageManager::_checkFileSize(const char* path) {
    if (!SD.exists(path)) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    size_t size = file.size();
    file.close();
    
    if (size > SD_MAX_FILE_SIZE) { 
        String timestamp = (_rtcManager && _rtcManager->isInitialized()) ? 
                          _rtcManager->getDateTime() : String(millis());
        timestamp.replace(" ", "_"); 
        timestamp.replace(":", "-");
        
        String backupPath = String(path) + "." + timestamp + ".bak";
        SD.rename(path, backupPath.c_str());
        
        Serial.printf("[StorageManager] Arquivo rotacionado: %s\n", backupPath.c_str());
        
        if (strcmp(path, SD_LOG_FILE) == 0) createTelemetryFile();
        else if (strcmp(path, SD_MISSION_FILE) == 0) createMissionFile();
        else if (strcmp(path, SD_SYSTEM_LOG) == 0) createLogFile();
    }
    return true;
}

void StorageManager::_formatTelemetryToCSV(const TelemetryData& data, char* buffer, size_t len) {
    if (buffer == nullptr || len < 100) return;
    buffer[0] = '\0';
    
    // Helper para float seguro
    auto sf = [](float v) -> float { 
        return (isnan(v) || isinf(v)) ? 0.0f : v; 
    };
    
    // Construir CSV em partes menores para evitar stack overflow no snprintf
    int pos = 0;
    
    // Parte 1: Timestamps e bateria
    pos += snprintf(buffer + pos, len - pos, "%lu,%lu,%lu,%.2f,%.1f,",
        data.timestamp, data.timestamp, data.missionTime,
        sf(data.batteryVoltage), sf(data.batteryPercentage));
    
    if (pos >= (int)len) return;
    
    // Parte 2: Temperaturas e pressao
    pos += snprintf(buffer + pos, len - pos, "%.2f,%.2f,%.2f,%.1f,",
        sf(data.temperature), sf(data.temperatureBMP), sf(data.temperatureSI), 
        sf(data.pressure));
    
    if (pos >= (int)len) return;
    
    // Parte 3: GPS
    pos += snprintf(buffer + pos, len - pos, "%.6f,%.6f,%.1f,%d,%d,",
        data.latitude, data.longitude, sf(data.gpsAltitude), 
        data.satellites, data.gpsFix ? 1 : 0);
    
    if (pos >= (int)len) return;
    
    // Parte 4: IMU
    pos += snprintf(buffer + pos, len - pos, "%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,",
        sf(data.gyroX), sf(data.gyroY), sf(data.gyroZ),
        sf(data.accelX), sf(data.accelY), sf(data.accelZ),
        sf(data.magX), sf(data.magY), sf(data.magZ));
    
    if (pos >= (int)len) return;
    
    // Parte 5: Ambiente e status
    pos += snprintf(buffer + pos, len - pos, "%.1f,%.0f,0x%02X,%d,-,",
        sf(data.humidity), sf(data.co2), sf(data.tvoc),
        data.systemStatus, data.errorCount);
    
    if (pos >= (int)len) return;
    
    // Parte 6: Sistema
    snprintf(buffer + pos, len - pos, "%lu,%d,%lu,%.1f",
        data.uptime, data.resetCount, data.minFreeHeap, sf(data.cpuTemp));
}

void StorageManager::_formatMissionToCSV(const MissionData& data, char* buffer, size_t len) {
    // FIX: Usar timestamp local para evitar race conditions
    char ts[24];
    unsigned long unixTime = data.collectionTime > 0 ? data.collectionTime : (unsigned long)(millis()/1000);
    snprintf(ts, sizeof(ts), "%lu", unixTime);
    
    snprintf(buffer, len, 
        "%s,%lu,%u,%.1f,%.1f,%.1f,%d,%d,%.2f,%u,%u,%lu,%lu",
        ts, 
        unixTime,
        data.nodeId, 
        data.soilMoisture, data.ambientTemp, data.humidity, 
        data.irrigationStatus,
        data.rssi, data.snr, 
        data.packetsReceived, data.packetsLost, data.lastLoraRx,
        data.nodeTimestamp,       
        data.collectionTime,      
        data.retransmissionTime   
    );
}
