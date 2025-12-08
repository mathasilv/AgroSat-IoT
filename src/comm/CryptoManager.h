/**
 * @file CryptoManager.h
 * @brief Gerenciador de Criptografia AES-128 para LoRa
 * @version 1.0.0 (MODERADO 4.1 - Criptografia Implementada)
 */

#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#include <Arduino.h>
#include "mbedtls/aes.h"
#include "config.h"

class CryptoManager {
public:
    /**
     * @brief Criptografa dados usando AES-128 ECB
     * @param plaintext Dados originais
     * @param len Tamanho dos dados (DEVE ser múltiplo de 16)
     * @param ciphertext Buffer de saída (mesmo tamanho que plaintext)
     * @return true se sucesso
     */
    static bool encrypt(const uint8_t* plaintext, size_t len, uint8_t* ciphertext);
    
    /**
     * @brief Descriptografa dados usando AES-128 ECB
     * @param ciphertext Dados criptografados
     * @param len Tamanho dos dados (DEVE ser múltiplo de 16)
     * @param plaintext Buffer de saída (mesmo tamanho que ciphertext)
     * @return true se sucesso
     */
    static bool decrypt(const uint8_t* ciphertext, size_t len, uint8_t* plaintext);
    
    /**
     * @brief Adiciona padding PKCS7 aos dados
     * @param data Dados originais
     * @param len Tamanho original
     * @param paddedData Buffer de saída (deve ter espaço para até len+16)
     * @return Tamanho dos dados com padding
     */
    static size_t addPadding(const uint8_t* data, size_t len, uint8_t* paddedData);
    
    /**
     * @brief Remove padding PKCS7 dos dados
     * @param paddedData Dados com padding
     * @param len Tamanho com padding
     * @return Tamanho real dos dados (sem padding)
     */
    static size_t removePadding(const uint8_t* paddedData, size_t len);
    
    /**
     * @brief Verifica se criptografia está habilitada (config.h)
     */
    static bool isEnabled() { return AES_ENABLED; }

private:
    // ATENÇÃO: Chave de exemplo! SUBSTITUIR por chave real em produção!
    // Gerar chave segura: openssl rand -hex 16
    static constexpr uint8_t AES_KEY[16] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
    };
};

#endif // CRYPTO_MANAGER_H