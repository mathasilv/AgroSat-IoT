/**
 * @file config.h
 * @brief Configurações globais - Versão 11.0.5 (Fix SD Configs)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Identificação
#define TEAM_ID 666

// ========== CONFIGURAÇÃO GPS ==========
#define GPS_RX_PIN 34 
#define GPS_TX_PIN 12 
#define GPS_BAUD_RATE 9600

// ========== LORA SX1276 (LilyGo T3 V1.6.1) ==========
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 23
#define LORA_DIO0 26 

// ========== SD CARD ==========
#define SD_CS 13
#define SD_MOSI 15
#define SD_MISO 2
#define SD_SCLK 14

// --- ARQUIVOS SD (RESTAURADOS) ---
#define SD_LOG_FILE "/telemetry.csv"
#define SD_MISSION_FILE "/mission.csv"
#define SD_ERROR_FILE "/errors.log"
#define SD_SYSTEM_LOG "/system.log"
#define SD_MAX_FILE_SIZE 5242880 // 5MB

// ========== I2C SENSORS ==========
#define SENSOR_I2C_SDA 21
#define SENSOR_I2C_SCL 22
#define I2C_FREQUENCY 100000    
#define I2C_TIMEOUT_MS 3000     

// ========== POWER MANAGEMENT ==========
#define BATTERY_PIN 35 
#define BATTERY_SAMPLES 16
#define BATTERY_VREF 3.6      
#define BATTERY_DIVIDER 2.0   

// ========== GPIO DISPONÍVEIS ==========
#ifndef LED_BUILTIN
#define LED_BUILTIN 25 
#endif
#define BUTTON_PIN 4                    

// ========== CONFIGURAÇÕES DE TIMING ==========
#define BUTTON_DEBOUNCE_TIME 50         
#define BUTTON_LONG_PRESS_TIME 3000     
#define WIFI_TIMEOUT_MS 10000 
#define HTTP_TIMEOUT_MS 5000

// ========== MODOS DE OPERAÇÃO ==========
enum OperationMode : uint8_t {
    MODE_INIT = 0, MODE_PREFLIGHT = 1, MODE_FLIGHT = 2,
    MODE_POSTFLIGHT = 3, MODE_SAFE = 4, MODE_ERROR = 5
};

struct ModeConfig {
    bool displayEnabled;
    bool serialLogsEnabled;
    bool sdLogsVerbose;
    bool loraEnabled;
    bool httpEnabled;
    uint32_t telemetrySendInterval;
    uint32_t storageSaveInterval;
    uint32_t beaconInterval;
};

const ModeConfig PREFLIGHT_CONFIG = { true, true, true, true, true, 20000, 1000, 0 };
const ModeConfig FLIGHT_CONFIG = { false, false, false, true, true, 60000, 10000, 0 };
const ModeConfig SAFE_CONFIG = { false, true, true, true, false, 120000, 300000, 180000 };

// ========== ENDEREÇOS I2C ==========
#define MPU9250_ADDRESS 0x69
#define BMP280_ADDR_1 0x76
#define SI7021_ADDRESS 0x40
#define CCS811_ADDR_1 0x5A
#define DS3231_ADDRESS 0x68

// ========== LIMITES DE VALIDAÇÃO ==========
#define TEMP_MIN_VALID -90.0
#define TEMP_MAX_VALID 100.0
#define PRESSURE_MIN_VALID 5.0
#define PRESSURE_MAX_VALID 1100.0
#define HUMIDITY_MIN_VALID 0.0
#define HUMIDITY_MAX_VALID 100.0
#define CO2_MIN_VALID 350.0
#define CO2_MAX_VALID 8192.0 
#define TVOC_MIN_VALID 0.0
#define TVOC_MAX_VALID 1200.0
#define MAG_MIN_VALID -4800.0
#define MAG_MAX_VALID 4800.0
#define ACCEL_MIN_VALID -16.0
#define ACCEL_MAX_VALID 16.0
#define GYRO_MIN_VALID -2000.0
#define GYRO_MAX_VALID 2000.0

// ========== LORA CONFIG ==========
#define LORA_FREQUENCY 915E6
#define LORA_SPREADING_FACTOR 7         
#define LORA_SPREADING_FACTOR_SAFE 12   
#define LORA_SIGNAL_BANDWIDTH 125E3     
#define LORA_CODING_RATE 5              
#define LORA_TX_POWER 20                
#define LORA_PREAMBLE_LENGTH 8          
#define LORA_SYNC_WORD 0x12             
#define LORA_CRC_ENABLED true
#define LORA_LDRO_ENABLED true                 
#define LORA_MAX_TX_TIME_MS 400
#define LORA_DUTY_CYCLE_PERCENT 10      
#define LORA_DUTY_CYCLE_WINDOW_MS 3600000 

// ========== CRIPTOGRAFIA ==========
#define AES_KEY_SIZE 16                 
#define AES_ENABLED true                

// ========== WIFI & HTTP ==========
#define WIFI_SSID "MATHEUS "
#define WIFI_PASSWORD "12213490"
#define HTTP_SERVER "obsat.org.br"
#define HTTP_PORT 443
#define HTTP_ENDPOINT "/teste_post/envio.php"

// ========== BUFFER SIZES ==========
#define JSON_MAX_SIZE 2048 
#define PAYLOAD_MAX_SIZE 250
#define MAX_GROUND_NODES 3              
#define NODE_TTL_MS 1800000             

// ========== SYSTEM ==========
#define MISSION_DURATION_MS 7200000 
#define WATCHDOG_TIMEOUT_PREFLIGHT 60   
#define WATCHDOG_TIMEOUT_FLIGHT 90      
#define WATCHDOG_TIMEOUT_SAFE 180       
#define SYSTEM_HEALTH_INTERVAL 10000
#define BATTERY_LOW 3.7
#define BATTERY_CRITICAL 3.3

// ========== RTC ==========
#ifndef NTP_SERVER_PRIMARY
#define NTP_SERVER_PRIMARY   "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"
#endif
#define RTC_TIMEZONE_OFFSET (-3 * 3600)

// ========== DEBUG SERIAL ==========
#define DEBUG_BAUDRATE 115200

#include "Globals.h"

extern bool currentSerialLogsEnabled; 

// Macros de Debug Thread-Safe
template <typename T>
inline void _debugPrintSafe(T val) {
    if (xSerialMutex != NULL) {
        if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.print(val);
            xSemaphoreGive(xSerialMutex);
        }
    }
}

template <typename T>
inline void _debugPrintlnSafe(T val) {
    if (xSerialMutex != NULL) {
        if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.println(val);
            xSemaphoreGive(xSerialMutex);
        }
    }
}

#define DEBUG_PRINT(x) if(currentSerialLogsEnabled){ _debugPrintSafe(x); }
#define DEBUG_PRINTLN(x) if(currentSerialLogsEnabled){ _debugPrintlnSafe(x); }
#define DEBUG_PRINTF(...) if(currentSerialLogsEnabled){ safePrintf(__VA_ARGS__); }

// ========== ESTRUTURAS DE DADOS ==========
enum class PacketPriority : uint8_t { CRITICAL=0, HIGH_PRIORITY=1, NORMAL=2, LOW_PRIORITY=3 };
enum SystemStatusErrors : uint8_t { STATUS_OK=0, STATUS_WIFI_ERROR=1, STATUS_SD_ERROR=2, STATUS_SENSOR_ERROR=4, STATUS_LORA_ERROR=8, STATUS_BATTERY_LOW=16, STATUS_BATTERY_CRIT=32, STATUS_TEMP_ALARM=64, STATUS_WATCHDOG=128 };

struct TelemetryData {
    unsigned long timestamp; unsigned long missionTime;
    float batteryVoltage; float batteryPercentage;
    float temperature; float temperatureBMP; float temperatureSI; float pressure;
    double latitude; double longitude; float gpsAltitude; uint8_t satellites; bool gpsFix;
    float gyroX, gyroY, gyroZ; float accelX, accelY, accelZ;
    float altitude; float humidity; float co2; float tvoc; float magX, magY, magZ;
    uint8_t systemStatus; uint16_t errorCount;
    char payload[PAYLOAD_MAX_SIZE];
    uint32_t uptime; uint16_t resetCount; uint8_t resetReason; uint32_t minFreeHeap; float cpuTemp;
};

struct MissionData {
    uint16_t nodeId; uint16_t sequenceNumber;
    float soilMoisture; float ambientTemp; float humidity; uint8_t irrigationStatus;
    int16_t rssi; float snr;
    uint16_t packetsReceived; uint16_t packetsLost; unsigned long lastLoraRx;
    uint32_t nodeTimestamp; unsigned long collectionTime; unsigned long retransmissionTime; 
    uint8_t priority; bool forwarded; char originalPayloadHex[20]; uint8_t payloadLength;
};

struct GroundNodeBuffer {
    MissionData nodes[MAX_GROUND_NODES]; uint8_t activeNodes;
    unsigned long lastUpdate[MAX_GROUND_NODES]; uint16_t totalPacketsCollected;
};

struct HttpQueueMessage { TelemetryData data; GroundNodeBuffer nodes; };
struct StorageQueueMessage { TelemetryData data; GroundNodeBuffer nodes; };

#endif // CONFIG_H