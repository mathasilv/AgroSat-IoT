/**
 * @file config.h
 * @brief Configurações globais do CubeSat AgroSat-IoT - OBSAT Fase 2
 * @version 2.0.3
 * @date 2025-11-01
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
#define FIRMWARE_VERSION    "2.0.3-FASE2"
#define BUILD_DATE          __DATE__
#define BUILD_TIME          __TIME__

// ============================================================================
// CONFIGURAÇÕES DE HARDWARE - LoRa32 V2.1_1.6
// ============================================================================

// Pinos OLED (I2C)
#define OLED_SDA            21
#define OLED_SCL            22

#ifndef OLED_RST
#define OLED_RST            -1
#endif

#define OLED_ADDRESS        0x3C

// Pinos LoRa (SPI)
#define LORA_SCK            5
#define LORA_MISO           19
#define LORA_MOSI           27
#define LORA_CS             18
#define LORA_RST            23
#define LORA_DIO0           26
#define LORA_FREQUENCY      433E6  // 433 MHz (ajustar conforme região)

// Pinos SD Card (SPI compartilhado com LoRa, CS diferente)
#define SD_CS               13
#define SD_MOSI             15
#define SD_MISO             2
#define SD_SCLK             14

// Pinos I2C para sensores (MPU6050 e BMP280)
#define SENSOR_I2C_SDA      21  // Compartilhado com OLED
#define SENSOR_I2C_SCL      22  // Compartilhado com OLED
#define I2C_FREQUENCY       400000  // 400kHz Fast Mode

// Pino de medição de bateria (ADC)
#define BATTERY_PIN         35  // ADC1_CH7
#define BATTERY_SAMPLES     10
#define BATTERY_VREF        3.3
#define BATTERY_DIVIDER     2.0  // Ajustar conforme divisor resistivo

// Pinos de controle
#ifndef LED_BUILTIN
#define LED_BUILTIN         25  // LED onboard
#endif

#define BUTTON_PIN          0   // Boot button

// ============================================================================
// CONFIGURAÇÕES DE SENSORES
// ============================================================================

// MPU6050 (Giroscópio e Acelerômetro)
#define MPU6050_ADDRESS     0x68
#define MPU6050_CALIBRATION_SAMPLES 100

// Intervalos de leitura (ms)
#define SENSOR_READ_INTERVAL    1000   // 1 segundo
#define TELEMETRY_SEND_INTERVAL 240000 // 4 minutos (240.000 ms)
#define STORAGE_SAVE_INTERVAL   60000  // 1 minuto

// ============================================================================
// CONFIGURAÇÕES DE COMUNICAÇÃO WiFi
// ============================================================================

// Configuração de rede (conforme OBSAT)
#define WIFI_SSID           "OBSAT_BALLOON"  // SSID do balão
#define WIFI_PASSWORD       "obsat2025"      // Senha (ajustar conforme edital)
#define WIFI_TIMEOUT_MS     30000            // 30 segundos timeout
#define WIFI_RETRY_ATTEMPTS 5

// HTTP Endpoint (conforme Apêndice 1 do edital)
#define HTTP_SERVER         "obsat.org.br"
#define HTTP_PORT           80
#define HTTP_ENDPOINT       "/teste/post/envio.php"
#define HTTP_TIMEOUT_MS     10000

// Formato JSON (máximo 90 bytes para payload)
#define JSON_MAX_SIZE       512
#define PAYLOAD_MAX_SIZE    90

// ============================================================================
// CONFIGURAÇÕES DE ARMAZENAMENTO
// ============================================================================

#define SD_LOG_FILE         "/telemetry.csv"
#define SD_MISSION_FILE     "/mission.csv"
#define SD_ERROR_FILE       "/errors.log"
#define SD_MAX_FILE_SIZE    10485760  // 10 MB

// ============================================================================
// CONFIGURAÇÕES DE POWER MANAGEMENT
// ============================================================================

#define BATTERY_MIN_VOLTAGE     3.3   // Volts
#define BATTERY_MAX_VOLTAGE     4.2   // Volts (Li-Ion)
#define BATTERY_CRITICAL        3.5   // Nível crítico
#define BATTERY_LOW             3.7   // Nível baixo

// Deep Sleep (não usado durante voo, mas disponível para testes)
#define DEEP_SLEEP_DURATION     3600  // 1 hora (segundos)

// ============================================================================
// CONFIGURAÇÕES DE SISTEMA
// ============================================================================

#define MISSION_DURATION_MS     7200000  // 2 horas (7.200.000 ms)
#define WATCHDOG_TIMEOUT        30       // segundos
#define SYSTEM_HEALTH_INTERVAL  10000    // 10 segundos

// Limites operacionais (conforme edital)
#define MAX_ALTITUDE            30000    // metros
#define MIN_TEMPERATURE         -80      // °C
#define MAX_TEMPERATURE         85       // °C

// ============================================================================
// CONFIGURAÇÕES DE DEBUG
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
// ESTRUTURAS DE DADOS
// ============================================================================

/**
 * @brief Estrutura de dados de telemetria obrigatória (OBSAT)
 */
struct TelemetryData {
    // Timestamp
    unsigned long timestamp;        // millis() desde boot
    unsigned long missionTime;      // tempo desde início da missão
    
    // Bateria (obrigatório)
    float batteryVoltage;           // Volts
    float batteryPercentage;        // %
    
    // Temperatura (obrigatório)
    float temperature;              // °C
    
    // Pressão (obrigatório)
    float pressure;                 // hPa
    float altitude;                 // metros (calculado)
    
    // Giroscópio (obrigatório - 3 eixos)
    float gyroX;                    // rad/s
    float gyroY;
    float gyroZ;
    
    // Acelerômetro (obrigatório - 3 eixos)
    float accelX;                   // m/s²
    float accelY;
    float accelZ;
    
    // Status do sistema
    uint8_t systemStatus;           // Bitmask de status
    uint16_t errorCount;            // Contador de erros
    
    // Payload customizado (máximo 90 bytes)
    char payload[PAYLOAD_MAX_SIZE];
};

/**
 * @brief Estrutura de dados da missão (LoRa IoT Agrícola)
 */
struct MissionData {
    // Dados recebidos via LoRa dos sensores terrestres
    float soilMoisture;             // Umidade do solo (%)
    float ambientTemp;              // Temperatura ambiente (°C)
    float humidity;                 // Umidade relativa do ar (%)
    uint8_t irrigationStatus;       // Status da irrigação (0/1)
    
    // Métricas de comunicação LoRa
    int16_t rssi;                   // RSSI do último pacote LoRa
    float snr;                      // SNR do último pacote LoRa
    uint16_t packetsReceived;       // Contador de pacotes recebidos
    uint16_t packetsLost;           // Contador de pacotes perdidos
    
    // Timestamp
    unsigned long lastLoraRx;       // Último pacote LoRa recebido
};

/**
 * @brief Status do sistema (bitmask)
 */
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

#endif // CONFIG_H
