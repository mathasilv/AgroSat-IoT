/**
 * @file config.h
 * @brief Configurações globais do CubeSat AgroSat-IoT - OBSAT Fase 2 + Sensores Expandidos
 * @version 2.1.0
 * @date 2025-11-04
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// IDENTIFICAÇÃO DA MISSÃO
// ============================================================================
#define MISSION_NAME        "AgroSat-IoT"
#define TEAM_NAME           "Orbitalis"
#define TEAM_CATEGORY       "N3"
#define FIRMWARE_VERSION    "2.1.0-EXPANDIDO"
#define BUILD_DATE          __DATE__
#define BUILD_TIME          __TIME__

// ============================================================================
// HARDWARE (LoRa32 V2.1_1.6) E I/O
// ============================================================================
#define OLED_SDA            21
#define OLED_SCL            22
#ifndef OLED_RST
#define OLED_RST            16
#endif
#define OLED_ADDRESS        0x3C

#define LORA_SCK            5
#define LORA_MISO           19
#define LORA_MOSI           27
#define LORA_CS             18
#define LORA_RST            23
#define LORA_DIO0           26
#define LORA_FREQUENCY      433E6

#define SD_CS               13
#define SD_MOSI             15
#define SD_MISO             2
#define SD_SCLK             14

#define SENSOR_I2C_SDA      21
#define SENSOR_I2C_SCL      22
#define I2C_FREQUENCY       400000

#define BATTERY_PIN         35
#define BATTERY_SAMPLES     10
#define BATTERY_VREF        3.3
#define BATTERY_DIVIDER     2.0

#ifndef LED_BUILTIN
#define LED_BUILTIN         25
#endif
#define BUTTON_PIN          0

// ============================================================================
// CONFIGURAÇÃO DE SENSORES (AUTO-DETECÇÃO + SELEÇÃO)
// ============================================================================

// Endereços I2C dos sensores
#define MPU6050_ADDRESS     0x68
#define MPU9250_ADDRESS     0x68
#define BMP280_ADDR_1       0x76
#define BMP280_ADDR_2       0x77
#define SHT20_ADDRESS       0x40
#define CCS811_ADDR_1       0x5A
#define CCS811_ADDR_2       0x5B

// Configurações de calibração e filtros
#define MPU6050_CALIBRATION_SAMPLES 100
#define CUSTOM_FILTER_SIZE          5

// Intervalos de leitura
#define SENSOR_READ_INTERVAL    1000
#define CCS811_READ_INTERVAL    5000  // CCS811 é mais lento
#define SHT20_READ_INTERVAL     2000  // SHT20 precisa tempo entre leituras

// ============================================================================
// REDE/HTTP/COMUNICAÇÃO
// ============================================================================
#define WIFI_SSID           "OBSAT_BALLOON"
#define WIFI_PASSWORD       "obsat2025"
#define WIFI_TIMEOUT_MS     30000
#define WIFI_RETRY_ATTEMPTS 5

#define HTTP_SERVER         "obsat.org.br"
#define HTTP_PORT           80
#define HTTP_ENDPOINT       "/teste/post/envio.php"
#define HTTP_TIMEOUT_MS     10000

#define JSON_MAX_SIZE       768  // Aumentado para acomodar campos extras
#define PAYLOAD_MAX_SIZE    90

// Intervalos de operação
#define TELEMETRY_SEND_INTERVAL 240000  // 4 minutos
#define STORAGE_SAVE_INTERVAL   60000   // 1 minuto

// ============================================================================
// ARMAZENAMENTO & SISTEMA
// ============================================================================
#define SD_LOG_FILE         "/telemetry.csv"
#define SD_MISSION_FILE     "/mission.csv"
#define SD_ERROR_FILE       "/errors.log"
#define SD_MAX_FILE_SIZE    10485760

#define BATTERY_MIN_VOLTAGE     3.3
#define BATTERY_MAX_VOLTAGE     4.2
#define BATTERY_CRITICAL        3.5
#define BATTERY_LOW             3.7

#define DEEP_SLEEP_DURATION     3600

#define MISSION_DURATION_MS     7200000  // 2 horas
#define WATCHDOG_TIMEOUT        30
#define SYSTEM_HEALTH_INTERVAL  10000

#define MAX_ALTITUDE            30000
#define MIN_TEMPERATURE         -80
#define MAX_TEMPERATURE         85

// ============================================================================
// DEBUG
// ============================================================================
#define DEBUG_SERIAL        Serial
#define DEBUG_BAUDRATE      115200
#define DEBUG_ENABLED       true

#if DEBUG_ENABLED
  #define DEBUG_PRINT(x)      DEBUG_SERIAL.print(x)
  #define DEBUG_PRINTLN(x)    DEBUG_SERIAL.println(x)
  #define DEBUG_PRINTF(...)   DEBUG_SERIAL.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ============================================================================
// ESTRUTURAS DE DADOS EXPANDIDAS
// ============================================================================

struct TelemetryData {
    // CAMPOS OBRIGATÓRIOS (OBSAT)
    unsigned long timestamp;
    unsigned long missionTime;
    float batteryVoltage;
    float batteryPercentage;
    float temperature;
    float pressure;
    float altitude;
    float gyroX, gyroY, gyroZ;
    float accelX, accelY, accelZ;
    uint8_t systemStatus;
    uint16_t errorCount;
    char payload[PAYLOAD_MAX_SIZE];
    
    // CAMPOS EXPANDIDOS (OPCIONAIS)
    float humidity;     // SHT20
    float co2;          // CCS811 (ppm)
    float tvoc;         // CCS811 (ppb)
    float magX, magY, magZ;  // MPU9250 magnetômetro (µT)
};

struct MissionData {
    // Dados da missão LoRa
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
    STATUS_OK           = 0x00,
    STATUS_WIFI_ERROR   = 0x01,
    STATUS_SD_ERROR     = 0x02,
    STATUS_SENSOR_ERROR = 0x04,
    STATUS_LORA_ERROR   = 0x08,
    STATUS_BATTERY_LOW  = 0x10,
    STATUS_BATTERY_CRIT = 0x20,
    STATUS_TEMP_ALARM   = 0x40,
    STATUS_WATCHDOG     = 0x80
};

// ============================================================================
// CONFIGURAÇÕES ESPECÍFICAS POR SENSOR
// ============================================================================

// Limites de validação para cada sensor
#define TEMP_MIN_VALID      -50.0
#define TEMP_MAX_VALID      100.0
#define PRESSURE_MIN_VALID  300.0  // hPa
#define PRESSURE_MAX_VALID  1100.0 // hPa
#define HUMIDITY_MIN_VALID  0.0
#define HUMIDITY_MAX_VALID  100.0
#define CO2_MIN_VALID       350.0
#define CO2_MAX_VALID       5000.0
#define TVOC_MIN_VALID      0.0
#define TVOC_MAX_VALID      1000.0

// Timeouts de inicialização
#define SENSOR_INIT_TIMEOUT     2000
#define CCS811_WARMUP_TIME      20000  // CCS811 precisa aquecer

#endif // CONFIG_H
