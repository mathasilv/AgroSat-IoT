/**
 * @file StorageManager.cpp
 * @brief Gerenciador de Armazenamento com CRC16 e Redundância Tripla
 * @version 3.0.0
 */

#include "StorageManager.h"
#include "core/RTCManager/RTCManager.h"

SPIClass spiSD(HSPI);
static char fmtBuffer[512];

StorageManager::StorageManager() : 
    _available(false), 
    _rtcManager(nullptr), 
    _lastInitAttempt(0),
    _crcErrors(0),
    _totalWrites(0) 
{}

bool StorageManager::begin() {
    Serial.println("[StorageManager] Inicializando SD Card...");
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

void StorageManager::_attemptRecovery() {
    if (_available) return;
    unsigned long now = millis();
    if (now - _lastInitAttempt < REINIT_INTERVAL) return;
    _lastInitAttempt = now;
    
    Serial.println("[StorageManager] Tentando recuperar SD Card...");
    SD.end();
    if (begin()) {
        Serial.println("[StorageManager] RECUPERADO!");
    }
}

// ============================================================================
// Salvamento Normal (CSV com CRC no final da linha)
// ============================================================================
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
    
    _formatTelemetryToCSV(data, fmtBuffer, sizeof(fmtBuffer));
    
    // NOVO 4.6: Calcular CRC16 da linha e adicionar ao final
    uint16_t crc = _calculateCRC16((uint8_t*)fmtBuffer, strlen(fmtBuffer));
    
    char lineWithCRC[600];
    snprintf(lineWithCRC, sizeof(lineWithCRC), "%s,%04X", fmtBuffer, crc);
    
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
        _available = false; 
        return false; 
    }
    
    _formatMissionToCSV(data, fmtBuffer, sizeof(fmtBuffer));
    
    // NOVO 4.6: CRC16
    uint16_t crc = _calculateCRC16((uint8_t*)fmtBuffer, strlen(fmtBuffer));
    
    char lineWithCRC[400];
    snprintf(lineWithCRC, sizeof(lineWithCRC), "%s,%04X", fmtBuffer, crc);
    
    file.println(lineWithCRC);
    file.close();
    
    _totalWrites++;
    return true;
}

// ============================================================================
// NOVO 5.8: Salvamento com Redundância Tripla
// ============================================================================
bool StorageManager::saveTelemetryRedundant(const TelemetryData& data) {
    if (!_available) { 
        _attemptRecovery(); 
        if (!_available) return false; 
    }
    
    // Serializar dados críticos em buffer
    uint8_t buffer[256];
    int offset = 0;
    
    memcpy(buffer + offset, &data.timestamp, sizeof(data.timestamp)); 
    offset += sizeof(data.timestamp);
    
    memcpy(buffer + offset, &data.batteryVoltage, sizeof(data.batteryVoltage)); 
    offset += sizeof(data.batteryVoltage);
    
    memcpy(buffer + offset, &data.temperature, sizeof(data.temperature)); 
    offset += sizeof(data.temperature);
    
    memcpy(buffer + offset, &data.systemStatus, sizeof(data.systemStatus)); 
    offset += sizeof(data.systemStatus);
    
    // Escrever 3 cópias com CRC individual
    return _writeTripleRedundant("/telemetry_critical.bin", buffer, offset);
}

bool StorageManager::saveMissionDataRedundant(const MissionData& data) {
    if (!_available) { 
        _attemptRecovery(); 
        if (!_available) return false; 
    }
    
    uint8_t buffer[128];
    int offset = 0;
    
    memcpy(buffer + offset, &data.nodeId, sizeof(data.nodeId)); 
    offset += sizeof(data.nodeId);
    
    memcpy(buffer + offset, &data.soilMoisture, sizeof(data.soilMoisture)); 
    offset += sizeof(data.soilMoisture);
    
    memcpy(buffer + offset, &data.rssi, sizeof(data.rssi)); 
    offset += sizeof(data.rssi);
    
    return _writeTripleRedundant("/mission_critical.bin", buffer, offset);
}

bool StorageManager::_writeTripleRedundant(const char* path, const uint8_t* data, size_t len) {
    File file = SD.open(path, FILE_APPEND);
    if (!file) return false;
    
    // Calcular CRC uma vez
    uint16_t crc = _calculateCRC16(data, len);
    
    // Escrever 3 cópias consecutivas: [DATA][CRC][DATA][CRC][DATA][CRC]
    for (int i = 0; i < 3; i++) {
        file.write(data, len);
        file.write((uint8_t*)&crc, sizeof(crc));
    }
    
    file.close();
    _totalWrites++;
    
    Serial.printf("[StorageManager] Escrita redundante: %d bytes x3\n", len);
    return true;
}

bool StorageManager::_readWithRedundancy(const char* path, uint8_t* data, size_t len) {
    // Implementação de leitura com votação majoritária (2 de 3)
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    
    uint8_t copy1[256], copy2[256], copy3[256];
    uint16_t crc1, crc2, crc3;
    
    // Ler 3 cópias
    file.read(copy1, len); file.read((uint8_t*)&crc1, sizeof(crc1));
    file.read(copy2, len); file.read((uint8_t*)&crc2, sizeof(crc2));
    file.read(copy3, len); file.read((uint8_t*)&crc3, sizeof(crc3));
    file.close();
    
    // Validar CRCs
    bool valid1 = (_calculateCRC16(copy1, len) == crc1);
    bool valid2 = (_calculateCRC16(copy2, len) == crc2);
    bool valid3 = (_calculateCRC16(copy3, len) == crc3);
    
    // Votação majoritária
    if (valid1 && valid2) { memcpy(data, copy1, len); return true; }
    if (valid1 && valid3) { memcpy(data, copy1, len); return true; }
    if (valid2 && valid3) { memcpy(data, copy2, len); return true; }
    
    // Se apenas uma válida, usar essa (melhor que nada)
    if (valid1) { memcpy(data, copy1, len); _crcErrors++; return true; }
    if (valid2) { memcpy(data, copy2, len); _crcErrors++; return true; }
    if (valid3) { memcpy(data, copy3, len); _crcErrors++; return true; }
    
    // Falha total
    _crcErrors += 3;
    return false;
}

// ============================================================================
// NOVO 4.6: Cálculo de CRC-16 (CCITT)
// ============================================================================
uint16_t StorageManager::_calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;  // Valor inicial
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;  // Polinômio CCITT
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

// ============================================================================
// Continuação de StorageManager.cpp - Métodos Auxiliares
// ============================================================================

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
    
    snprintf(fmtBuffer, sizeof(fmtBuffer), "[%s] %s", ts.c_str(), message.c_str());
    
    uint16_t crc = _calculateCRC16((uint8_t*)fmtBuffer, strlen(fmtBuffer));
    
    char lineWithCRC[600];
    snprintf(lineWithCRC, sizeof(lineWithCRC), "%s,%04X", fmtBuffer, crc);
    
    file.println(lineWithCRC);
    file.close();
    return true;
}

bool StorageManager::logError(const String& errorMsg) { 
    return saveLog("[ERROR] " + errorMsg); 
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
    file.println(F("Uptime,ResetCnt,MinHeap,CpuTemp,CRC16"));  // NOVO: Colunas Health + CRC
    file.close();
    return true;
}

bool StorageManager::createMissionFile() {
    if (SD.exists(SD_MISSION_FILE)) return true;
    File file = SD.open(SD_MISSION_FILE, FILE_WRITE);
    if (!file) return false;
    
    file.print(F("ISO8601,UnixTimestamp,NodeID,SoilMoisture,AmbTemp,Humidity,"));
    file.print(F("Irrigation,RSSI,SNR,PktsRx,PktsLost,LastRx,"));
    file.println(F("NodeOriginTS,SatArrivalTS,SatTxTS,CRC16"));  // NOVO: CRC
    file.close();
    return true;
}

bool StorageManager::createLogFile() {
    if (SD.exists(SD_SYSTEM_LOG)) return true;
    File file = SD.open(SD_SYSTEM_LOG, FILE_WRITE);
    if (!file) return false;
    
    file.println(F("=== AGROSAT-IOT SYSTEM LOG v3.0 ==="));
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
        
        // Recriar arquivo
        if (strcmp(path, SD_LOG_FILE) == 0) createTelemetryFile();
        else if (strcmp(path, SD_MISSION_FILE) == 0) createMissionFile();
        else if (strcmp(path, SD_SYSTEM_LOG) == 0) createLogFile();
    }
    return true;
}

void StorageManager::_formatTelemetryToCSV(const TelemetryData& data, char* buffer, size_t len) {
    auto safeF = [](float v) { return isnan(v) ? 0.0f : v; };
    
    String ts = (_rtcManager && _rtcManager->isInitialized()) ? 
                _rtcManager->getDateTime() : String(millis());
    
    snprintf(buffer, len,
        "%s,%lu,%lu,%.2f,%.1f,%.2f,%.2f,%.2f,%.2f,%.1f,"
        "%.6f,%.6f,%.1f,%d,%d,"
        "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,"
        "%.1f,%.0f,%.0f,0x%02X,%d,%s,"
        "%lu,%d,%lu,%.1f",
        ts.c_str(), (unsigned long)data.timestamp, (unsigned long)data.missionTime,
        data.batteryVoltage, data.batteryPercentage,
        safeF(data.temperature), safeF(data.temperatureBMP), safeF(data.temperatureSI), 
        safeF(data.pressure), safeF(data.altitude),
        data.latitude, data.longitude, safeF(data.gpsAltitude), 
        data.satellites, data.gpsFix,
        safeF(data.gyroX), safeF(data.gyroY), safeF(data.gyroZ),
        safeF(data.accelX), safeF(data.accelY), safeF(data.accelZ),
        safeF(data.magX), safeF(data.magY), safeF(data.magZ),
        safeF(data.humidity), safeF(data.co2), safeF(data.tvoc),
        data.systemStatus, data.errorCount, data.payload,
        data.uptime, data.resetCount, data.minFreeHeap, data.cpuTemp  // NOVO: Health
    );
}

void StorageManager::_formatMissionToCSV(const MissionData& data, char* buffer, size_t len) {
    String ts = (_rtcManager && _rtcManager->isInitialized()) ? 
                _rtcManager->getDateTime() : String(millis());
    
    snprintf(buffer, len, 
        "%s,%lu,%u,%.1f,%.1f,%.1f,%d,%d,%.2f,%u,%u,%lu,%u,%lu,%lu",
        ts.c_str(), 
        (_rtcManager && _rtcManager->isInitialized()) ? 
            _rtcManager->getUnixTime() : (unsigned long)(millis()/1000),
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

void StorageManager::listFiles() {
    if (!_available) {
        Serial.println("[StorageManager] SD Card não disponível.");
        return;
    }
    
    Serial.println("[StorageManager] === Arquivos no SD Card ===");
    File root = SD.open("/");
    File file = root.openNextFile();
    
    while (file) {
        Serial.printf("  %s - %d bytes\n", file.name(), file.size());
        file = root.openNextFile();
    }
    
    Serial.println("========================================");
}

uint64_t StorageManager::getFreeSpace() { 
    return _available ? SD.totalBytes() - SD.usedBytes() : 0; 
}

uint64_t StorageManager::getUsedSpace() { 
    return _available ? SD.usedBytes() : 0; 
}