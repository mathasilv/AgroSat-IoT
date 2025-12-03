/**
 * @file GPSManager.h
 * @brief Gerenciador do Módulo GPS NEO-M8N
 * @details Encapsula o TinyGPS++ e gerencia a UART1
 */

#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <TinyGPSPlus.h> // Certifique-se de instalar esta lib no platformio.ini
#include "config.h"

class GPSManager {
public:
    GPSManager();

    /**
     * @brief Inicializa a UART1 nos pinos definidos em config.h
     * @return true sempre (inicialização de serial não retorna erro no ESP32)
     */
    bool begin();

    /**
     * @brief Processa dados da Serial. Deve ser chamado no loop principal.
     */
    void update();

    // Getters de Dados Processados
    double getLatitude() const { return _latitude; }
    double getLongitude() const { return _longitude; }
    float getAltitude() const { return _altitude; }
    uint8_t getSatellites() const { return _satellites; }
    
    /**
     * @brief Verifica se há um FIX válido (localização atualizada recentemente)
     */
    bool hasFix() const { return _hasFix; }
    
    /**
     * @brief Retorna o tempo em ms desde a última sentença válida decodificada
     */
    uint32_t getLastFixAge() const;

private:
    TinyGPSPlus _gps;
    HardwareSerial* _serial;

    // Cache de dados
    double _latitude;
    double _longitude;
    float _altitude;
    uint8_t _satellites;
    bool _hasFix;
    
    uint32_t _lastEncoded; // Timestamp da última decodificação válida
};

#endif