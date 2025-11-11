#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// Estados do display
enum DisplayState {
    DISPLAY_BOOT,           // Tela de inicialização
    DISPLAY_INIT_SENSORS,   // Status de inicialização dos sensores
    DISPLAY_CALIBRATION,    // Calibração do IMU
    DISPLAY_READY,          // Sistema pronto
    DISPLAY_TELEMETRY_1,    // Tela 1: Temperatura, Pressão, Altitude
    DISPLAY_TELEMETRY_2,    // Tela 2: IMU (Gyro + Accel)
    DISPLAY_TELEMETRY_3,    // Tela 3: Magnetômetro + Umidade
    DISPLAY_TELEMETRY_4,    // Tela 4: Qualidade do ar (CO2 + TVOC)
    DISPLAY_STATUS          // Status geral do sistema
};

class DisplayManager {
public:
    DisplayManager();
    bool begin();
    
    // Métodos de exibição
    void showBoot();
    void showSensorInit(const char* sensorName, bool status);
    void showCalibration(uint8_t progress);
    void showCalibrationResult(float offsetX, float offsetY, float offsetZ);
    void showReady();
    
    // Atualização de telemetria (rotação automática)
    void updateTelemetry(const TelemetryData& data);
    
    // Forçar mudança de tela
    void nextScreen();
    void setScreen(DisplayState state);
    
    // Utilitários
    void clear();
    void displayMessage(const char* title, const char* msg);
    
    // ✅ NOVO: Desligamento físico total
    void turnOff();
    void turnOn();
    bool isOn() const { return _isDisplayOn; }

private:
    Adafruit_SSD1306 _display;
    DisplayState _currentState;
    DisplayState _lastTelemetryScreen;
    uint32_t _lastScreenChange;
    uint32_t _screenInterval;
    bool _isDisplayOn;  // ✅ NOVO: Flag de controle
    
    // Métodos internos de renderização
    void _showTelemetry1(const TelemetryData& data);
    void _showTelemetry2(const TelemetryData& data);
    void _showTelemetry3(const TelemetryData& data);
    void _showTelemetry4(const TelemetryData& data);
    void _showStatus(const TelemetryData& data);
    
    // Utilitários de desenho
    void _drawHeader(const char* title);
    void _drawProgressBar(uint8_t progress, const char* label);
};

#endif
