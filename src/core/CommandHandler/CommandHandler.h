#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include "sensors/SensorManager/SensorManager.h"

class CommandHandler {
public:
    explicit CommandHandler(SensorManager& sensors);
    bool handle(String cmd); // String por cópia é ok aqui (curtas)

private:
    SensorManager& _sensors;
    
    void _printHelp();
    void _handleSensorCmd(const String& cmd);
};

#endif