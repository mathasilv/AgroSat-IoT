/**
 * @file config.h
 * @brief Configurações globais do CubeSat AgroSat-IoT - Store-and-Forward LEO
 * @version 7.1.1 (Correção de Compilação)
 * @date 2025-12-02
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Identificação
#define TEAM_ID 666

// ========== LORA SX1276 (TTGO LoRa32) ==========
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 23
#define LORA_DIO0 26
#define LORA_DIO1 33
#define LORA_DIO2 32

// ========== SD CARD ==========
#define SD_CS 13
#define SD_MOSI 15
#define SD_MISO 2
#define SD_SCLK 14

// ========== I2C SENSORS ==========
#define SENSOR_I2C_SDA 21
#define SENSOR_I2C_SCL 22
#define I2C_FREQUENCY 50000   // 50kHz (Robustez CCS811)
#define I2C_TIMEOUT_MS 3000   // 3s (Clock Stretching CCS811)

// ========== POWER MANAGEMENT ==========
#define BATTERY_PIN 35
#define BATTERY_SAMPLES 16
#define BATTERY_VREF 3.6      
#define BATTERY_DIVIDER 2.0   

// ========== GPIO DISPONÍVEIS ==========
#ifndef LED_BUILTIN
#define LED_BUILTIN 25
#endif

// ========== BOTÃO DE CONTROLE ==========
#define BUTTON_PIN 4                    
#define BUTTON_DEBOUNCE_TIME 50         
#define BUTTON_LONG_PRESS_TIME 3000     

// ========== MODOS DE OPERAÇÃO ==========
enum OperationMode : uint8_t {
    MODE_INIT = 0,
    MODE_PREFLIGHT = 1,
    MODE_FLIGHT = 2,
    MODE_POSTFLIGHT = 3,
    MODE_SAFE = 4,
    MODE_ERROR = 5
};

// ========== CONFIGURAÇÕES DE MODO ==========
struct ModeConfig {
    bool displayEnabled;
    bool serialLogsEnabled;
    bool sdLogsVerbose;
    bool loraEnabled;
    bool httpEnabled;
    uint32_t telemetrySendInterval;
    uint32_t storageSaveInterval;
};

// Configuração Pré-Voo (Debug total)
const ModeConfig PREFLIGHT_CONFIG = {
    true,    // display
    true,    // serialLogs
    true,    // sdLogs
    true,    // lora
    true,    // http
    60000,   // envio a cada 60s
    10000    // salva no SD a cada 10s
};

// Configuração de Voo (Otimizado)
const ModeConfig FLIGHT_CONFIG = {
    false,   // display OFF
    false,   // serialLogs OFF
    false,   // sdLogs verbose OFF
    true,    // lora ON
    true,    // http ON
    60000,   // envio 60s
    10000    // salva SD 10s
};

// Configuração de Segurança (Bateria Crítica/Erro)
const ModeConfig SAFE_CONFIG = {
    false,
    true,    // logs ON para diagnóstico
    true,
    true,    // lora ON (Beacon)
    false,   // http OFF
    120000,  // envio lento (2 min)
    300000   // salva SD lento (5 min)
};

// ========== ENDEREÇOS I2C ==========
#define MPU9250_ADDRESS 0x69
#define BMP280_ADDR_1 0x76
#define SI7021_ADDRESS 0x40
#define CCS811_ADDR_1 0x5A
#define DS3231_ADDRESS 0x68

// ========== LIMITES DE VALIDAÇÃO ==========
#define TEMP_MIN_VALID -50.0
#define TEMP_MAX_VALID 100.0
#define PRESSURE_MIN_VALID 300.0
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

// ========== LORA ==========
#define LORA_FREQUENCY 915E6
#define LORA_SPREADING_FACTOR 7         
#define LORA_SPREADING_FACTOR_SAFE 12   
#define LORA_SIGNAL_BANDWIDTH 125E3     
#define LORA_CODING_RATE 5              
#define LORA_TX_POWER 20                
#define LORA_PREAMBLE_LENGTH 8          
#define LORA_SYNC_WORD 0x12             
#define LORA_CRC_ENABLED true           
#define LORA_MAX_TX_TIME_MS 400
#define LORA_MIN_INTERVAL_MS 20000
#define LORA_MAX_PAYLOAD_SIZE 255       
#define LORA_TX_TIMEOUT_MS_NORMAL 2000   
#define LORA_TX_TIMEOUT_MS_SAFE   5000   

// ========== WIFI & HTTP ==========
#define WIFI_SSID "MATHEUS "
#define WIFI_PASSWORD "12213490"
#define WIFI_TIMEOUT_MS 10000 
#define WIFI_RETRY_ATTEMPTS 3

#define HTTP_SERVER "obsat.org.br"
#define HTTP_PORT 443
#define HTTP_ENDPOINT "/teste_post/envio.php"
#define HTTP_TIMEOUT_MS 5000

// ========== BUFFERS & REDE (Correção de Erros) ==========
#define JSON_MAX_SIZE 2048 
#define PAYLOAD_MAX_SIZE 250
#define MAX_GROUND_NODES 5              // Necessário para GroundNodeManager
#define NODE_TTL_MS 1800000             // 30 min (TTL de nós no buffer)
#define NODE_INACTIVITY_TIMEOUT_MS 600000 

// ========== INTERVALOS ==========
#define TELEMETRY_SEND_INTERVAL PREFLIGHT_CONFIG.telemetrySendInterval
#define STORAGE_SAVE_INTERVAL PREFLIGHT_CONFIG.storageSaveInterval

// ========== SD CARD - ARQUIVOS ==========
#define SD_LOG_FILE "/telemetry.csv"
#define SD_MISSION_FILE "/mission.csv"
#define SD_ERROR_FILE "/errors.log"
#define SD_SYSTEM_LOG "/system.log"
#define SD_MAX_FILE_SIZE 5242880 // 5MB

// ========== BATERIA ==========
#define BATTERY_MIN_VOLTAGE 3.5
#define BATTERY_MAX_VOLTAGE 4.2
#define BATTERY_CRITICAL 3.6
#define BATTERY_LOW 3.7

// ========== SISTEMA ==========
#define MISSION_DURATION_MS 7200000 // 2 horas
#define WATCHDOG_TIMEOUT 60         // 60 segundos
#define SYSTEM_HEALTH_INTERVAL 10000

// ========== RTC & NTP ==========
#ifndef NTP_SERVER_PRIMARY
#define NTP_SERVER_PRIMARY   "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"
#endif
#define RTC_TIMEZONE_OFFSET (-3 * 3600)  // UTC-3 Brasil

// ========== DEBUG SERIAL ==========
#define DEBUG_SERIAL Serial
#define DEBUG_BAUDRATE 115200
extern bool currentSerialLogsEnabled; 

#define DEBUG_PRINT(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.print(x);}
#define DEBUG_PRINTLN(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.println(x);}
#define DEBUG_PRINTF(...) if(currentSerialLogsEnabled){DEBUG_SERIAL.printf(__VA_ARGS__);}

// ========== ESTRUTURAS DE DADOS ==========
struct TelemetryData {
    unsigned long timestamp;
    unsigned long missionTime;
    
    float batteryVoltage;
    float batteryPercentage;
    
    float temperature;    
    float temperatureBMP; 
    float temperatureSI;  
    float pressure;
    
    float gyroX, gyroY, gyroZ;
    float accelX, accelY, accelZ;
    
    float altitude;
    float humidity;
    float co2;
    float tvoc;
    
    float magX, magY, magZ;
    
    uint8_t systemStatus;
    uint16_t errorCount;
    
    char payload[PAYLOAD_MAX_SIZE];
};

struct MissionData {
    uint16_t nodeId;
    uint16_t sequenceNumber;
    float soilMoisture;
    float ambientTemp;
    float humidity;
    uint8_t irrigationStatus;
    int16_t rssi;
    float snr;
    uint16_t packetsReceived;
    uint16_t packetsLost;
    unsigned long lastLoraRx;
    unsigned long collectionTime;
    uint8_t priority;
    bool forwarded;
    char originalPayloadHex[20];
    uint8_t payloadLength;
};

struct GroundNodeBuffer {
    MissionData nodes[MAX_GROUND_NODES];
    uint8_t activeNodes;
    unsigned long lastUpdate[MAX_GROUND_NODES];
    uint16_t totalPacketsCollected;
};

enum SystemStatusErrors : uint8_t {
    STATUS_OK = 0x00,
    STATUS_WIFI_ERROR = 0x01,
    STATUS_SD_ERROR = 0x02,
    STATUS_SENSOR_ERROR = 0x04,
    STATUS_LORA_ERROR = 0x08,
    STATUS_BATTERY_LOW = 0x10,
    STATUS_BATTERY_CRIT = 0x20,
    STATUS_TEMP_ALARM = 0x40,
    STATUS_WATCHDOG = 0x80
};

#endif // CONFIG_H