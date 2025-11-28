#include "AirQualityService.h"
#include "config.h"     // se suas macros DEBUG_* vêm daqui


AirQualityService::AirQualityService(CCS811Hal& ccs811)
    : _ccs811(ccs811),
      _online(false),
      _warmedUp(false),
      _warmupStart(0),
      _lastRead(0),
      _co2(NAN),
      _tvoc(NAN)
{
}

bool AirQualityService::begin(float tempC, float humRel) {
    _online      = false;
    _warmedUp    = false;
    _warmupStart = millis();
    _lastRead    = 0;
    _co2         = NAN;
    _tvoc        = NAN;

    DEBUG_PRINTLN("[AirQualityService] Inicializando CCS811...");

    if (!_ccs811.begin(CCS811_ADDR_1)) {
        DEBUG_PRINTLN("[AirQualityService] Falha no begin() do CCS811");
        return false;
    }

    DEBUG_PRINTLN("[AirQualityService] CCS811: Aguardando warmup (20s)...");
    _online = true;

    // Se já tiver T/RH válidos, aplica logo a compensação
    _applyEnvCompensation(tempC, humRel);

    return true;
}

void AirQualityService::update(float tempC, float humRel) {
    if (!_online) return;

    uint32_t now = millis();

    // Warmup obrigatório de 20 s (datasheet) [web:365]
    if (!_warmedUp) {
        if (now - _warmupStart < CCS811_WARMUP_TIME) {
            // durante warmup, só vai atualizando compensação ambiental
            _applyEnvCompensation(tempC, humRel);
            return;
        }
        DEBUG_PRINTLN("[AirQualityService] Warmup concluído, começando leituras regulares");
        _warmedUp = true;
    }

    if (now - _lastRead < CCS811_READ_INTERVAL) {
        // também pode atualizar a compensação de vez em quando
        _applyEnvCompensation(tempC, humRel);
        return;
    }
    _lastRead = now;

    // Atualiza compensação de T/RH antes da leitura [web:365]
    _applyEnvCompensation(tempC, humRel);

    if (_ccs811.available() && _ccs811.readData() == 0) {
        float co2  = _ccs811.geteCO2();
        float tvoc = _ccs811.getTVOC();

        if (_validateReadings(co2, tvoc)) {
            _co2  = co2;
            _tvoc = tvoc;
        }
    }
}

bool AirQualityService::_validateReadings(float co2, float tvoc) {
    if (isnan(co2) || isnan(tvoc)) return false;

    // Mesmos limites que você já usava em _validateCCSReadings() [attached_file:295]
    if (co2  < CO2_MIN_VALID  || co2  > CO2_MAX_VALID)  return false;
    if (tvoc < TVOC_MIN_VALID || tvoc > TVOC_MAX_VALID) return false;

    return true;
}

void AirQualityService::_applyEnvCompensation(float tempC, float humRel) {
    if (isnan(tempC) || isnan(humRel)) return;

    // Datasheet recomenda fornecer T/RH reais para melhorar a acurácia. [web:365][web:359]
    _ccs811.setEnvironmentalData(humRel, tempC);
    DEBUG_PRINTF("[AirQualityService] Compensação T=%.1f°C H=%.1f%%\n", tempC, humRel);
}
