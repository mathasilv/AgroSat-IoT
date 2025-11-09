/**
 * @file config.h
 * @brief Configurações globais do CubeSat AgroSat-IoT - OBSAT Fase 2
 * @version 2.3.0 (OBSAT compliant)
 * @date 2025-11-09
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// IDENTIFICAÇÃO DA MISSÃO
// ============================================================================
#define MISSION_NAME "AgroSat-IoT"
#define TEAM_CATEGORY "N3"
#define FIRMWARE_VERSION "2.3.0"
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
#define LORA_FREQUENCY 433E6

#define SD_CS 13
#define SD_MOSI 15
#define SD_MISO 2
#define SD_SCLK 14

#define SENSOR_I2C_SDA 21
#define SENSOR_I2C_SCL 22
#define I2C_FREQUENCY 100000

#define BATTERY_PIN 35
#define BATTERY_SAMPLES 10
#define BATTERY_VREF 3.3
#define BATTERY_DIVIDER 2.0

#ifndef LED_BUILTIN
#define LED_BUILTIN 25
#endif
#define BUTTON_PIN 0

// ============================================================================
// MODOS OPERACIONAIS E CONFIGS DINÂMICAS
// ============================================================================
enum OperationMode : uint8_t {
    MODE_PREFLIGHT = 0,
    MODE_FLIGHT = 1,
    MODE_POSTFLIGHT = 2,
    MODE_ERROR = 3,
    MODE_INIT = 4
};

struct ModeConfig {
    bool displayEnabled;
    bool serialLogsEnabled;
    bool sdLogsVerbose;
    uint32_t telemetrySendInterval;
    uint32_t storageSaveInterval;
    uint8_t wifiDutyCycle; // 0-100%
};

// Configurações para cada modo
const ModeConfig PREFLIGHT_CONFIG = {
    true,   // displayEnabled
    true,   // serialLogsEnabled
    true,   // sdLogsVerbose
    30000,  // telemetrySendInterval (30s para testes)
    60000,  // storageSaveInterval (1min)
    100     // wifiDutyCycle (sempre ligado)
};

const ModeConfig FLIGHT_CONFIG = {
    false,   // displayEnabled (economia de energia)
    false,   // serialLogsEnabled (economia de energia)
    false,   // sdLogsVerbose (economia de energia)
    240000,  // telemetrySendInterval (4min - requisito OBSAT)
    240000,  // storageSaveInterval (4min)
    5        // wifiDutyCycle (5% - liga só para enviar)
};

// ============================================================================
// CONFIGURAÇÃO DE SENSORES (AUTO-DETECÇÃO + SELEÇÃO)
// ============================================================================
#define MPU6050_ADDRESS 0x68
#define MPU9250_ADDRESS 0x68
#define BMP280_ADDR_1 0x76
#define BMP280_ADDR_2 0x77
#define SHT20_ADDRESS 0x40
#define CCS811_ADDR_1 0x5A
#define CCS811_ADDR_2 0x5B
#define MPU6050_CALIBRATION_SAMPLES 100
#define CUSTOM_FILTER_SIZE 5
#define SENSOR_READ_INTERVAL 1000
#define CCS811_READ_INTERVAL 5000
#define SHT20_READ_INTERVAL 2000

// ============================================================================
// REDE/HTTP/COMUNICAÇÃO - OBSAT
// ============================================================================
#define WIFI_SSID "MATHEUS "
#define WIFI_PASSWORD "12213490"
#define WIFI_TIMEOUT_MS 60000
#define WIFI_RETRY_ATTEMPTS 5

// Servidor OBSAT oficial (HTTPS obrigatório)
#define HTTP_SERVER "obsat.org.br"
#define HTTP_PORT 443
#define HTTP_ENDPOINT "/teste_post/envio.php"
#define HTTP_TIMEOUT_MS 10000

// Limites de tamanho JSON (OBSAT)
#define JSON_MAX_SIZE 512      // Tamanho total do JSON (servidor aceita até ~500 bytes)
#define PAYLOAD_MAX_SIZE 90    // Limite OBSAT para campo payload

// Intervalos de envio (seguem modo operacional)
#define TELEMETRY_SEND_INTERVAL PREFLIGHT_CONFIG.telemetrySendInterval
#define STORAGE_SAVE_INTERVAL PREFLIGHT_CONFIG.storageSaveInterval

// ============================================================================
// ARMAZENAMENTO & SISTEMA
// ============================================================================
#define SD_LOG_FILE "/telemetry.csv"
#define SD_MISSION_FILE "/mission.csv"
#define SD_ERROR_FILE "/errors.log"
#define SD_MAX_FILE_SIZE 10485760

#define BATTERY_MIN_VOLTAGE 3.3
#define BATTERY_MAX_VOLTAGE 4.2
#define BATTERY_CRITICAL 3.5
#define BATTERY_LOW 3.7

#define DEEP_SLEEP_DURATION 3600
#define MISSION_DURATION_MS 7200000  // 2 horas (requisito OBSAT)
#define WATCHDOG_TIMEOUT 60
#define SYSTEM_HEALTH_INTERVAL 10000

#define MAX_ALTITUDE 30000
#define MIN_TEMPERATURE -80
#define MAX_TEMPERATURE 85

// ============================================================================
// DEBUG (segue valor do modo operacional)
// ============================================================================
#define DEBUG_SERIAL Serial
#define DEBUG_BAUDRATE 115200
extern bool currentSerialLogsEnabled;
#define DEBUG_PRINT(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.print(x);}
#define DEBUG_PRINTLN(x) if(currentSerialLogsEnabled){DEBUG_SERIAL.println(x);}
#define DEBUG_PRINTF(...) if(currentSerialLogsEnabled){DEBUG_SERIAL.printf(__VA_ARGS__);}

// ============================================================================
// ESTRUTURAS DE DADOS - COMPATÍVEL OBSAT
// ============================================================================

/**
 * @brief Estrutura de telemetria compatível com servidor OBSAT
 * @note Campos obrigatórios OBSAT:
 *       - equipe (número) - TEAM_ID
 *       - bateria (número) - batteryPercentage
 *       - temperatura (número) - temperature
 *       - pressao (número) - pressure
 *       - giroscopio (array [x,y,z]) - gyroX, gyroY, gyroZ
 *       - acelerometro (array [x,y,z]) - accelX, accelY, accelZ
 *       - payload (string JSON, máx 90 bytes) - montado dinamicamente
 */
struct TelemetryData {
    // Campos de sistema
    unsigned long timestamp;
    unsigned long missionTime;
    
    // Campos obrigatórios OBSAT
    float batteryVoltage;
    float batteryPercentage;  // Enviado como "bateria"
    float temperature;        // Enviado como "temperatura"
    float pressure;           // Enviado como "pressao"
    
    // Giroscópio (convertido para array [x,y,z] no JSON)
    float gyroX, gyroY, gyroZ;
    
    // Acelerômetro (convertido para array [x,y,z] no JSON)
    float accelX, accelY, accelZ;
    
    // Dados adicionais para payload customizado
    float altitude;
    float humidity;
    float co2;
    float tvoc;
    
    // Magnetômetro (opcional, para payload)
    float magX, magY, magZ;
    
    // Status do sistema
    uint8_t systemStatus;
    uint16_t errorCount;
    
    // Payload legado (compatibilidade, não usado para OBSAT)
    char payload[PAYLOAD_MAX_SIZE];
};

/**
 * @brief Dados de missão específicos (LoRa, sensores de solo, etc)
 */
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

/**
 * @brief Status do sistema (bitmask)
 */
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

// ============================================================================
// TIMEOUTS DE INICIALIZAÇÃO
// ============================================================================
#define SENSOR_INIT_TIMEOUT 2000
#define CCS811_WARMUP_TIME 20000

#endif // CONFIG_H
