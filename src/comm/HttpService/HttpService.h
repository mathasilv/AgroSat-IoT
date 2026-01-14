/**
 * @file HttpService.h
 * @brief Serviço de comunicação HTTP/HTTPS para envio de telemetria
 * 
 * @details Implementa cliente HTTP para envio de dados JSON:
 *          - Suporte a HTTPS com verificação de certificado
 *          - Timeout configurável
 *          - Retry automático em caso de falha
 *          - Compatível com API REST padrão
 * 
 * @author AgroSat Team
 * @date 2024
 * @version 1.0.0
 * 
 * @copyright Copyright (c) 2024 AgroSat Project
 * @license MIT License
 * 
 * ## Configuração
 * | Parâmetro     | Valor (config.h)    |
 * |---------------|---------------------|
 * | HTTP_SERVER   | obsat.org.br        |
 * | HTTP_PORT     | 443 (HTTPS)         |
 * | HTTP_ENDPOINT | /teste_post/envio.php |
 * | HTTP_TIMEOUT  | 5000ms              |
 * 
 * ## Formato do Payload
 * @code{.json}
 * {
 *   "equipe": 666,
 *   "bateria": 85,
 *   "temperatura": "25.50",
 *   "pressao": "1013.25",
 *   ...
 * }
 * @endcode
 * 
 * @note Requer WiFi conectado antes de usar
 * @see WiFiService para gerenciamento de conexão
 * @see PayloadManager::createTelemetryJSON() para criação do payload
 */

#ifndef HTTP_SERVICE_H
#define HTTP_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @class HttpService
 * @brief Cliente HTTP para envio de telemetria em JSON
 */
class HttpService {
public:
    /**
     * @brief Construtor padrão
     */
    HttpService();
    
    /**
     * @brief Envia payload JSON via POST
     * @param jsonPayload String JSON formatada
     * @return true se servidor respondeu 200/201
     * @return false se erro de conexão ou resposta != 2xx
     * @note Bloqueante - aguarda resposta ou timeout
     */
    bool postJson(const String& jsonPayload);
};

#endif // HTTP_SERVICE_H