/**
 * @file config.h
 * @brief Configurações globais do CubeSat AgroSat-IoT - VERSÃO OTIMIZADA
 * @version 4.1.0
 * @date 2025-11-11
 * 
 * CHANGELOG v4.1.0:
 * - [NEW] Controle de LoRa e HTTP por modo operacional
 * - [NEW] Flags loraEnabled e httpEnabled nos ModeConfig
 * - [NEW] Comandos serial para controle: LORA ON/OFF, HTTP ON/OFF
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// IDENTIFICAÇÃO DA MISSÃO
// ============================================================================
#define MISSION_NAME "AgroSat-IoT"
#define TEAM_CATEGORY "N3"
#define FIRMWARE_VERSION "4.1.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
#define TEAM_ID 666

// ============================================================================
// HARDWARE (LoRa32 V2.1_1.6) E I/O
// ============================================================================
#define OLED_SDA 21
#define OLED_SCL 22
#ifndef OLED_RST
#define OLED_RST 16
#endif
#define OLED_ADDRESS 0x3C

#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 23
#define LORA_DIO0 26
#define LORA_FREQUENCY 915E6

#define SD_CS 13
#define SD_MOSI 15
#define SD_MISO 2
#define SD_SCLK 14

#define SENSOR_I2C_SDA 21
#define SENSOR_I2C_SCL 22
#define I2C_FREQUENCY 100000

#define BATTERY_PIN 35
#define BATTERY_SAMPLES 16
#define BATTERY_VREF 3.6
#define BATTERY_DIVIDER 2.0

#ifndef LED_BUILTIN
#define LED_BUILTIN 25
#endif
#define BUTTON_PIN 0

// ============================================================================
// MODOS OPERACIONAIS E CONFIGS DINÂMICAS
// ============================================================================
enum OperationMode : uint8_t {
    MODE_INIT = 0,
    MODE_PREFLIGHT = 1,
    MODE_FLIGHT = 2,
    MODE_POSTFLIGHT = 3,
    MODE_SAFE = 4,
    MODE_ERROR = 5
};

struct ModeConfig {
    bool displayEnabled;
    bool serialLogsEnabled;
    bool sdLogsVerbose;
    bool loraEnabled;        // ✅ NOVO: Controle LoRa
    bool httpEnabled;        // ✅ NOVO: Controle HTTP
    uint32_t telemetrySendInterval;
    uint32_t storageSaveInterval;
    uint8_t wifiDutyCycle;
};

// ✅ PREFLIGHT: Tudo habilitado para testes
const ModeConfig PREFLIGHT_CONFIG = {
    true,   // displayEnabled
    true,   // serialLogsEnabled
    true,   // sdLogsVerbose
    false,   // loraEnabled - LoRa ATIVO
    false,   // httpEnabled - HTTP ATIVO
    30000,  // telemetrySendInterval (30s)
    60000,  // storageSaveInterval (1min)
    100     // wifiDutyCycle (sempre ligado)
};

// ✅ FLIGHT: Operação eficiente, ambos ativos
const ModeConfig FLIGHT_CONFIG = {
    false,   // displayEnabled
    false,   // serialLogsEnabled
    false,   // sdLogsVerbose
    true,    // loraEnabled - LoRa ATIVO (missão)
    true,    // httpEnabled - HTTP ATIVO (missão)
    240000,  // telemetrySendInterval (4min - OBSAT)
    240000,  // storageSaveInterval (4min)
    5        // wifiDutyCycle (5%)
};

// ✅ SAFE: Modo degradado, APENAS LoRa (economia extrema)
const ModeConfig SAFE_CONFIG = {
    false,   // displayEnabled
    true,    // serialLogsEnabled (manter debug)
    true,    // sdLogsVerbose
    true,    // loraEnabled - LoRa ATIVO (comunicação crítica)
    false,   // httpEnabled - HTTP DESLIGADO (economia)
    120000,  // telemetrySendInterval (2min)
    300000,  // storageSaveInterval (5min)
    0        // wifiDutyCycle (desligar WiFi)
};

// ============================================================================
// CONFIGURAÇÃO DE SENSORES - PLACA PION REAL
// ============================================================================
#define USE_MPU9250
#define USE_BMP280
#define USE_SI7021
#define USE_CCS811

// Endereços I2C
#define MPU9250_ADDRESS 0x69
#define BMP280_ADDR_1 0x76
#define BMP280_ADDR_2 0x77
#define SI7021_ADDRESS 0x40
#define CCS811_ADDR_1 0x5A
#define CCS811_ADDR_2 0x5B
#define DS3231_ADDRESS 0x68

// Intervalos de leitura
#define CUSTOM_FILTER_SIZE 5
#define SENSOR_READ_INTERVAL 1000
#define CCS811_READ_INTERVAL 5000
#define SI7021_READ_INTERVAL 2000
#define MPU9250_CALIBRATION_SAMPLES 100
#define CCS811_WARMUP_TIME 20000

// ============================================================================
// LIMITES DE VALIDAÇÃO DE SENSORES
// ============================================================================
#define TEMP_MIN_VALID -50.0
#define TEMP_MAX_VALID 100.0
#define PRESSURE_MIN_VALID 300.0
#define PRESSURE_MAX_VALID 1100.0
#define HUMIDITY_MIN_VALID 0.0
#define HUMIDITY_MAX_VALID 100.0
#define CO2_MIN_VALID 350.0
#define CO2_MAX_VALID 5000.0
#define TVOC_MIN_VALID 0.0
#define TVOC_MAX_VALID 1000.0
#define MAG_MIN_VALID -4800.0
#define MAG_MAX_VALID 4800.0
#define ACCEL_MIN_VALID -16.0
#define ACCEL_MAX_VALID 16.0
#define GYRO_MIN_VALID -2000.0
#define GYRO_MAX_VALID 2000.0

// ============================================================================
// TIMEOUTS DE INICIALIZAÇÃO
// ============================================================================
#define SENSOR_INIT_TIMEOUT 2000

// ============================================================================
// COMUNICAÇÃO - LORA (ANATEL COMPLIANCE)
// ============================================================================
#define LORA_MAX_TX_TIME_MS 2000
#define LORA_DUTY_CYCLE_PERCENT 1.0
#define LORA_MIN_INTERVAL_MS ((uint32_t)(LORA_MAX_TX_TIME_MS * (100.0 / LORA_DUTY_CYCLE_PERCENT)))

#define LORA_SPREADING_FACTOR 12
#define LORA_SIGNAL_BANDWIDTH 125E3
#define LORA_CODING_RATE 8
#define LORA_TX_POWER 20
#define LORA_SYNC_WORD 0x34

// ============================================================================
// REDE/HTTP/COMUNICAÇÃO - OBSAT
// ============================================================================
#define WIFI_SSID "MATHEUS "
#define WIFI_PASSWORD "12213490"
#define WIFI_TIMEOUT_MS 60000
#define WIFI_RETRY_ATTEMPTS 5

#define HTTP_SERVER "obsat.org.br"
#define HTTP_PORT 443
#define HTTP_ENDPOINT "/teste_post/envio.php"
#define HTTP_TIMEOUT_MS 10000

#define JSON_MAX_SIZE 512
#define PAYLOAD_MAX_SIZE 90

#define TELEMETRY_SEND_INTERVAL PREFLIGHT_CONFIG.telemetrySendInterval
#define STORAGE_SAVE_INTERVAL PREFLIGHT_CONFIG.storageSaveInterval

// ============================================================================
// ARMAZENAMENTO & SISTEMA
// ============================================================================
#define SD_LOG_FILE "/telemetry.csv"
#define SD_MISSION_FILE "/mission.csv"
#define SD_ERROR_FILE "/errors.log"
#define SD_MAX_FILE_SIZE 10485760

#define BATTERY_MIN_VOLTAGE 3.7
#define BATTERY_MAX_VOLTAGE 4.2
#define BATTERY_CRITICAL 3.9
#define BATTERY_LOW 3.8

#define DEEP_SLEEP_DURATION 3600
#define MISSION_DURATION_MS 7200000
#define WATCHDOG_TIMEOUT 60
#define SYSTEM_HEALTH_INTERVAL 10000

#define MAX_ALTITUDE 30000
#define MIN_TEMPERATURE -80
#define MAX_TEMPERATURE 85

// NTP
#define NTP_SERVER_PRIMARY   "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.google.com"
#define NTP_SERVER_TERTIARY  "time.cloudflare.com"
#define TIMEZONE_STRING      "<-03>3"
#define RTC_TIMEZONE_OFFSET -10800

// ============================================================================
// DEBUG (segue modo operacional)
// ============================================================================
#define DEBUG_SERIAL Serial
#define DEBUG_BAUDRATE 115200
extern bool currentSerialLogsEnabled;
#define DEBUG_PRINT(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.print(x);}
#define DEBUG_PRINTLN(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.println(x);}
#define DEBUG_PRINTF(...) if(currentSerialLogsEnabled){DEBUG_SERIAL.printf(__VA_ARGS__);}

// ============================================================================
// BUFFER CIRCULAR TELEMETRIA
// ============================================================================
#define TELEMETRY_BUFFER_SIZE 10

// ============================================================================
// ESTRUTURAS DE DADOS - COMPATÍVEL OBSAT
// ============================================================================
struct TelemetryData {
    unsigned long timestamp;
    unsigned long missionTime;
    
    float batteryVoltage;
    float batteryPercentage;
    float temperature;
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
    float soilMoisture;
    float ambientTemp;
    float humidity;
    uint8_t irrigationStatus;
    int16_t rssi;
    float snr;
    uint16_t packetsReceived;
    uint16_t packetsLost;
    unsigned long lastLoraRx;
};

enum SystemStatus : uint8_t {
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
