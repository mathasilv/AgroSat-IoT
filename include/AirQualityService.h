#ifndef AIR_QUALITY_SERVICE_H
#define AIR_QUALITY_SERVICE_H

#include <Arduino.h>
#include "CCS811Hal.h"
#include "config.h"

// Serviço de alto nível para o CCS811 (eCO2 + TVOC) [web:365][web:362]
class AirQualityService {
public:
    explicit AirQualityService(CCS811Hal& ccs811);

    // Inicializa o sensor e executa warmup obrigatório (20 s).
    // tempC/humRel podem ser NAN; se válidos, já são usados na compensação. [web:365]
    bool begin(float tempC = NAN, float humRel = NAN);

    // Deve ser chamado periodicamente.
    // tempC/humRel são usados para atualizar a compensação ambiental. [web:365]
    void update(float tempC = NAN, float humRel = NAN);

    bool  isOnline() const { return _online; }
    float getCO2()   const { return _co2;   }   // ppm
    float getTVOC()  const { return _tvoc;  }   // ppb

private:
    CCS811Hal&   _ccs811;
    bool         _online;
    bool         _warmedUp;
    uint32_t     _warmupStart;
    uint32_t     _lastRead;

    float        _co2;
    float        _tvoc;

    bool  _validateReadings(float co2, float tvoc);
    void  _applyEnvCompensation(float tempC, float humRel);
};

#endif // AIR_QUALITY_SERVICE_H
