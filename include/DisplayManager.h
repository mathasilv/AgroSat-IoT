/**
 * @file DisplayManager.h
 * @brief Gerenciador de display OLED SSD1306 128x64 - Versão Serial Monitor
 * @version 7.2.0 - Calibração com Feedback Visual
 */
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

enum DisplayState {
    DISPLAY_BOOT,
    DISPLAY_INIT_SENSORS,
    DISPLAY_CALIBRATION,
    DISPLAY_READY,
    DISPLAY_TELEMETRY_1,
    DISPLAY_TELEMETRY_2,
    DISPLAY_TELEMETRY_3,
    DISPLAY_TELEMETRY_4,
    DISPLAY_STATUS,
    DISPLAY_ERROR
};

class DisplayManager {
public:
    DisplayManager();
    
    bool begin();
    void clear();
    void turnOff();
    void turnOn();
    bool isOn() const { return _isDisplayOn; }
    
    void showBoot();
    void showSensorInit(const char* sensorName, bool status);
    void showCalibration(uint8_t progress);
    void showCalibrationResult(float offsetX, float offsetY, float offsetZ);
    void showReady();
    
    void updateTelemetry(const TelemetryData& data);
    void displayMessage(const char* title, const char* msg);
    
    void nextScreen();
    void setScreen(DisplayState state);

private:
    Adafruit_SSD1306 _display;
    DisplayState _currentState;
    DisplayState _lastTelemetryScreen;
    unsigned long _lastScreenChange;
    unsigned long _lastUpdateTime;
    uint32_t _screenInterval;
    uint8_t _scrollOffset;
    uint8_t _initLineCount;
    bool _isDisplayOn;
    
    void _drawProgressBar(uint8_t progress, const char* label);
    void _showTelemetry1(const TelemetryData& data);
    void _showTelemetry2(const TelemetryData& data);
    void _showTelemetry3(const TelemetryData& data);
    void _showTelemetry4(const TelemetryData& data);
    void _showStatus(const TelemetryData& data);
};

#endif
