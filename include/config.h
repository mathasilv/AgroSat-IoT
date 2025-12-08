/**
 * @file config.h
 * @brief Configurações globais do CubeSat AgroSat-IoT - Store-and-Forward LEO
 * @version 9.0.0 (Correções Críticas Implementadas)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Identificação
#define TEAM_ID 666

// ========== CONFIGURAÇÃO GPS (CORRIGIDO - CRÍTICO 2) ==========
#define GPS_RX_PIN 16      // U2_RXD (Serial2 padrão ESP32)
#define GPS_TX_PIN 17      // U2_TXD (ou -1 se GPS só TX)
#define GPS_BAUD_RATE 9600

// ========== LORA SX1276 ==========
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

// ========== I2C SENSORS (CORRIGIDO - CRÍTICO 3) ==========
#define SENSOR_I2C_SDA 21
#define SENSOR_I2C_SCL 22
#define I2C_FREQUENCY 100000    // 100kHz Standard Mode (CORRIGIDO de 50kHz)
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
    uint32_t beaconInterval;  // NOVO - 5.4
};

const ModeConfig PREFLIGHT_CONFIG = { true, true, true, true, true, 20000, 1000, 0 };
const ModeConfig FLIGHT_CONFIG = { false, false, false, true, true, 60000, 10000, 0 };
const ModeConfig SAFE_CONFIG = { false, true, true, true, false, 120000, 300000, 180000 }; // Beacon a cada 3min

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
#define LORA_LDRO_ENABLED true          // NOVO - 4.2           
#define LORA_MAX_TX_TIME_MS 400
#define LORA_MIN_INTERVAL_MS 20000
#define LORA_MAX_PAYLOAD_SIZE 255       
#define LORA_TX_TIMEOUT_MS_NORMAL 2000   
#define LORA_TX_TIMEOUT_MS_SAFE   5000

// ========== LORA DUTY CYCLE (NOVO - 4.8) ==========
#define LORA_DUTY_CYCLE_PERCENT 10      // 10% (ANATEL 915MHz)
#define LORA_DUTY_CYCLE_WINDOW_MS 3600000  // 1 hora

// ========== LINK BUDGET (NOVO - 4.2) ==========
#define LINK_MARGIN_MIN_DB 3.0f         // Margem mínima viável
#define MAX_COMM_DISTANCE_KM 2302.0f    // Distância máxima LoRa LEO
#define EARTH_RADIUS_KM 6371.0f
#define ORBITAL_ALTITUDE_KM 600.0f      // Assumindo órbita típica

// ========== CRIPTOGRAFIA (NOVO - 4.1) ==========
#define AES_KEY_SIZE 16                 // AES-128
#define AES_ENABLED true                // Habilitar criptografia

// ========== WIFI & HTTP ==========
#define WIFI_SSID "MATHEUS "
#define WIFI_PASSWORD "12213490"
#define WIFI_TIMEOUT_MS 10000 
#define WIFI_RETRY_ATTEMPTS 3

#define HTTP_SERVER "obsat.org.br"
#define HTTP_PORT 443
#define HTTP_ENDPOINT "/teste_post/envio.php"
#define HTTP_TIMEOUT_MS 5000

// ========== BUFFERS & REDE ==========
#define JSON_MAX_SIZE 2048 
#define PAYLOAD_MAX_SIZE 250
#define MAX_GROUND_NODES 3              
#define NODE_TTL_MS 1800000             
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

// ========== SISTEMA (CORRIGIDO - 4.5) ==========
#define MISSION_DURATION_MS 7200000 
#define WATCHDOG_TIMEOUT_PREFLIGHT 60   // 60s (NOVO)
#define WATCHDOG_TIMEOUT_FLIGHT 90      // 90s (NOVO)
#define WATCHDOG_TIMEOUT_SAFE 180       // 180s (NOVO - Corrigido)
#define SYSTEM_HEALTH_INTERVAL 10000

// ========== RTC & NTP ==========
#ifndef NTP_SERVER_PRIMARY
#define NTP_SERVER_PRIMARY   "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"
#endif
#define RTC_TIMEZONE_OFFSET (-3 * 3600)

// ========== DEBUG SERIAL ==========
#define DEBUG_SERIAL Serial
#define DEBUG_BAUDRATE 115200
extern bool currentSerialLogsEnabled; 

#define DEBUG_PRINT(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.print(x);}
#define DEBUG_PRINTLN(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.println(x);}
#define DEBUG_PRINTF(...) if(currentSerialLogsEnabled){DEBUG_SERIAL.printf(__VA_ARGS__);}

// ========== QoS PRIORITY (NOVO - 5.3) ==========
enum class PacketPriority : uint8_t {
    CRITICAL = 0,  // Alertas de irrigação, pragas
    HIGH = 1,      // Dados de sensores críticos
    NORMAL = 2,    // Telemetria regular
    LOW = 3        // Logs, debug
};

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
    
    double latitude;
    double longitude;
    float gpsAltitude;
    uint8_t satellites;
    bool gpsFix;

    float gyroX, gyroY, gyroZ;
    float accelX, accelY, accelZ;
    
    float altitude; // Barométrica
    float humidity;
    float co2;
    float tvoc;
    
    float magX, magY, magZ;
    
    uint8_t systemStatus;
    uint16_t errorCount;
    
    char payload[PAYLOAD_MAX_SIZE];
    
    // NOVO - 5.6 Health Telemetry
    uint32_t uptime;
    uint16_t resetCount;
    uint8_t resetReason;
    uint32_t minFreeHeap;
    float cpuTemp;
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
    
    uint32_t nodeTimestamp;           
    unsigned long collectionTime;     
    unsigned long retransmissionTime; 

    uint8_t priority;  // Usando PacketPriority
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

// ========== LINK BUDGET STRUCTURE (NOVO - 4.2) ==========
struct LinkBudget {
    float maxDistance;      // km
    float currentDistance;  // km (calculado via GPS)
    float linkMargin;       // dB
    float pathLoss;         // dB
    bool isViable;          // true se margin > 3dB
    int8_t recommendedSF;   // SF recomendado baseado em distância
};

#endif // CONFIG_H