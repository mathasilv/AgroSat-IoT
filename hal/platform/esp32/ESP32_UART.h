/**
 * @file ESP32_UART.h
 * @brief ESP32 Implementation of UART HAL
 * @details Concrete implementation using HardwareSerial
 * 
 * @author AgroSat-IoT Team
 * @date 2025-11-27
 */

#ifndef ESP32_UART_H
#define ESP32_UART_H

#include "hal/interface/UART.h"
#include <HardwareSerial.h>

namespace HAL {

/**
 * @class ESP32_UART
 * @brief ESP32-specific UART implementation
 */
class ESP32_UART : public UART {
private:
    HardwareSerial* serial;
    bool initialized;
    
public:
    /**
     * @brief Constructor
     * @param serialInstance Pointer to Serial, Serial1, or Serial2
     */
    explicit ESP32_UART(HardwareSerial* serialInstance = &Serial)
        : serial(serialInstance), initialized(false) {}
    
    ~ESP32_UART() override {
        if (initialized) {
            end();
        }
    }
    
    bool begin(uint32_t baudRate, int8_t txPin, int8_t rxPin) override {
        if (txPin >= 0 && rxPin >= 0) {
            serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);
        } else {
            serial->begin(baudRate);
        }
        initialized = true;
        return true;
    }
    
    size_t write(uint8_t data) override {
        if (!initialized) return 0;
        return serial->write(data);
    }
    
    size_t write(const uint8_t* buffer, size_t length) override {
        if (!initialized) return 0;
        return serial->write(buffer, length);
    }
    
    int available() override {
        if (!initialized) return 0;
        return serial->available();
    }
    
    int read() override {
        if (!initialized) return -1;
        return serial->read();
    }
    
    size_t read(uint8_t* buffer, size_t length) override {
        if (!initialized) return 0;
        return serial->readBytes(buffer, length);
    }
    
    void flush() override {
        if (initialized) {
            serial->flush();
        }
    }
    
    void end() override {
        if (initialized) {
            serial->end();
            initialized = false;
        }
    }
};

} // namespace HAL

#endif // ESP32_UART_H