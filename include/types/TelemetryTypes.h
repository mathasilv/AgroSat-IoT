/**
 * @file TelemetryTypes.h
 * @brief Definições de estruturas de dados para telemetria
 * 
 * @details Arquivo central com todas as estruturas de dados usadas
 *          no sistema AgroSat-IoT para comunicação entre módulos:
 *          - TelemetryData: Dados dos sensores locais
 *          - MissionData: Dados dos ground nodes
 *          - GroundNodeBuffer: Buffer de nós coletados
 *          - Mensagens de fila para tasks assíncronas
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.1.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Tamanhos de Estruturas
 * | Estrutura        | Tamanho Aprox. | Uso                    |
 * |------------------|----------------|------------------------|
 * | TelemetryData    | ~160 bytes     | Dados locais           |
 * | MissionData      | ~80 bytes      | Dados de ground node   |
 * | GroundNodeBuffer | ~260 bytes     | Buffer de 3 nós        |
 * | HttpQueueMessage | ~420 bytes     | Fila HTTP              |
 * 
 * @note Estruturas otimizadas para minimizar uso de RAM
 * @note payload[] reduzido de 250 para 64 bytes
 */

#ifndef TELEMETRY_TYPES_H
#define TELEMETRY_TYPES_H

#include <stdint.h>

//=============================================================================
// STATUS DO SISTEMA (BITMASK)
//=============================================================================

/**
 * @enum SystemStatusErrors
 * @brief Flags de erro do sistema (combinável via OR)
 */
enum SystemStatusErrors : uint8_t {
    STATUS_OK = 0,            ///< Sistema OK
    STATUS_WIFI_ERROR = 1,    ///< Erro de WiFi
    STATUS_SD_ERROR = 2,      ///< Erro de SD Card
    STATUS_SENSOR_ERROR = 4,  ///< Erro de sensor
    STATUS_LORA_ERROR = 8,    ///< Erro de LoRa
    STATUS_BATTERY_LOW = 16,  ///< Bateria baixa (<3.7V)
    STATUS_BATTERY_CRIT = 32, ///< Bateria crítica (<3.3V)
    STATUS_TEMP_ALARM = 64,   ///< Alarme de temperatura
    STATUS_WATCHDOG = 128     ///< Reset por watchdog
};

//=============================================================================
// PRIORIDADE DE PACOTES (QoS)
//=============================================================================

/**
 * @enum PacketPriority
 * @brief Níveis de prioridade para transmissão de pacotes
 */
enum class PacketPriority : uint8_t {
    CRITICAL = 0,      ///< Crítico: solo seco, temp extrema
    HIGH_PRIORITY = 1, ///< Alto: link ruim, irrigação ativa
    NORMAL = 2,        ///< Normal: operação padrão
    LOW_PRIORITY = 3   ///< Baixo: dados antigos
};

//=============================================================================
// DADOS DE TELEMETRIA (SENSORES LOCAIS)
//=============================================================================

/**
 * @struct TelemetryData
 * @brief Estrutura principal com dados de todos os sensores locais
 */
struct TelemetryData {
    //--- Timestamps ---
    unsigned long timestamp;      ///< millis() da coleta
    unsigned long missionTime;    ///< Tempo desde início da missão
    
    //--- Bateria ---
    float batteryVoltage;         ///< Tensão (V)
    float batteryPercentage;      ///< Carga (%)
    
    //--- Temperaturas ---
    float temperature;            ///< Temperatura fusionada (°C)
    float temperatureBMP;         ///< Temperatura BMP280 (°C)
    float temperatureSI;          ///< Temperatura SI7021 (°C)
    
    //--- Barômetro ---
    float pressure;               ///< Pressão atmosférica (hPa)
    float altitude;               ///< Altitude barométrica (m)
    
    //--- GPS ---
    double latitude;              ///< Latitude (graus decimais)
    double longitude;             ///< Longitude (graus decimais)
    float gpsAltitude;            ///< Altitude GPS (m)
    uint8_t satellites;           ///< Satélites no fix
    bool gpsFix;                  ///< Fix válido?
    
    //--- IMU: Giroscópio (°/s) ---
    float gyroX, gyroY, gyroZ;
    
    //--- IMU: Acelerômetro (g) ---
    float accelX, accelY, accelZ;
    
    //--- IMU: Magnetômetro (µT) ---
    float magX, magY, magZ;
    
    //--- Ambiente ---
    float humidity;               ///< Umidade relativa (%)
    float co2;                    ///< eCO2 (ppm)
    float tvoc;                   ///< TVOC (ppb)
    
    //--- Status ---
    uint8_t systemStatus;         ///< Flags de erro (bitmask)
    uint16_t errorCount;          ///< Contador de erros
    
    //--- Sistema ---
    uint32_t uptime;              ///< Uptime (ms)
    uint16_t resetCount;          ///< Contagem de resets
    uint8_t resetReason;          ///< Razão do último reset
    uint32_t minFreeHeap;         ///< Menor heap livre (bytes)
    float cpuTemp;                ///< Temperatura CPU (°C)
    
    //--- Payload ---
    char payload[64];             ///< Payload customizado
};

//=============================================================================
// DADOS DE MISSÃO (GROUND NODES)
//=============================================================================

/**
 * @struct MissionData
 * @brief Dados recebidos de um ground node via LoRa
 */
struct MissionData {
    //--- Identificação ---
    uint16_t nodeId;              ///< ID único do nó
    uint16_t sequenceNumber;      ///< Número de sequência
    
    //--- Sensores do Nó ---
    float soilMoisture;           ///< Umidade do solo (%)
    float ambientTemp;            ///< Temperatura ambiente (°C)
    float humidity;               ///< Umidade do ar (%)
    uint8_t irrigationStatus;     ///< Status irrigação (0/1)
    
    //--- Qualidade do Link ---
    int16_t rssi;                 ///< RSSI do pacote (dBm)
    float snr;                    ///< SNR do pacote (dB)
    uint16_t packetsReceived;     ///< Pacotes recebidos deste nó
    uint16_t packetsLost;         ///< Pacotes perdidos
    unsigned long lastLoraRx;     ///< Timestamp última recepção
    
    //--- Timestamps ---
    uint32_t nodeTimestamp;       ///< Timestamp do nó (se disponível)
    unsigned long collectionTime; ///< Quando foi coletado
    unsigned long retransmissionTime; ///< Quando foi retransmitido
    
    //--- Controle ---
    uint8_t priority;             ///< Prioridade QoS calculada
    bool forwarded;               ///< Já foi encaminhado?
    char originalPayloadHex[20];  ///< Payload original (hex)
    uint8_t payloadLength;        ///< Tamanho do payload
};

//=============================================================================
// BUFFER DE GROUND NODES
//=============================================================================

#ifndef MAX_GROUND_NODES
#define MAX_GROUND_NODES 3  ///< Máximo de nós simultâneos
#endif

/**
 * @struct GroundNodeBuffer
 * @brief Buffer circular para armazenar ground nodes ativos
 */
struct GroundNodeBuffer {
    MissionData nodes[MAX_GROUND_NODES];       ///< Array de nós
    uint8_t activeNodes;                       ///< Quantidade de nós ativos
    unsigned long lastUpdate[MAX_GROUND_NODES];///< Timestamps por slot
    uint16_t totalPacketsCollected;            ///< Total de pacotes coletados
};

//=============================================================================
// MENSAGENS DE FILA (TASKS ASSÍNCRONAS)
//=============================================================================

/**
 * @struct HttpQueueMessage
 * @brief Mensagem para fila de envio HTTP
 */
struct HttpQueueMessage {
    TelemetryData data;       ///< Dados de telemetria
    GroundNodeBuffer nodes;   ///< Buffer de ground nodes
};

/**
 * @struct StorageQueueMessage
 * @brief Mensagem para fila de armazenamento SD
 * @note Atualmente usa apenas sinal (1 byte) na fila real
 */
struct StorageQueueMessage {
    TelemetryData data;       ///< Dados de telemetria
    GroundNodeBuffer nodes;   ///< Buffer de ground nodes
};

//=============================================================================
// TELEMETRIA DE SAÚDE DO SISTEMA
//=============================================================================

/**
 * @struct HealthTelemetry
 * @brief Dados básicos de saúde do sistema
 */
struct HealthTelemetry {
    uint32_t uptime;          ///< Uptime em ms
    uint32_t freeHeap;        ///< Heap livre atual (bytes)
    uint32_t minFreeHeap;     ///< Menor heap livre registrado
    uint16_t resetCount;      ///< Contagem de resets
    uint8_t resetReason;      ///< Razão do último reset
    float cpuTemp;            ///< Temperatura da CPU (°C)
    uint8_t systemStatus;     ///< Flags de status
    uint16_t errorCount;      ///< Contador de erros
};

#endif // TELEMETRY_TYPES_H