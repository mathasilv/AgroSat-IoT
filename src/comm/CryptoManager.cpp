// ARQUIVO: src/comm/CryptoManager.cpp

/**
 * @file CryptoManager.cpp
 * @brief Implementação de Criptografia AES-128
 */

#include "CryptoManager.h"

// ============================================================================
// CORREÇÃO: Definição externa da chave AES (requerido pelo linker)
// ============================================================================
constexpr uint8_t CryptoManager::AES_KEY[16];

bool CryptoManager::encrypt(const uint8_t* plaintext, size_t len, uint8_t* ciphertext) {
    // Validação: tamanho deve ser múltiplo de 16 (bloco AES)
    if (len == 0 || len % 16 != 0) {
        DEBUG_PRINTF("[Crypto] ERRO: Tamanho inválido (%d). Deve ser múltiplo de 16.\n", len);
        return false;
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Configurar chave para encriptação
    int ret = mbedtls_aes_setkey_enc(&aes, AES_KEY, 128);
    if (ret != 0) {
        DEBUG_PRINTF("[Crypto] ERRO ao configurar chave: %d\n", ret);
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Criptografar bloco por bloco (16 bytes cada)
    for (size_t i = 0; i < len; i += 16) {
        ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, 
                                     plaintext + i, ciphertext + i);
        if (ret != 0) {
            DEBUG_PRINTF("[Crypto] ERRO ao criptografar bloco %d: %d\n", i/16, ret);
            mbedtls_aes_free(&aes);
            return false;
        }
    }
    
    mbedtls_aes_free(&aes);
    
    DEBUG_PRINTF("[Crypto] Criptografado %d bytes\n", len);
    return true;
}

bool CryptoManager::decrypt(const uint8_t* ciphertext, size_t len, uint8_t* plaintext) {
    // Validação
    if (len == 0 || len % 16 != 0) {
        DEBUG_PRINTF("[Crypto] ERRO: Tamanho inválido (%d). Deve ser múltiplo de 16.\n", len);
        return false;
    }
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Configurar chave para decriptação
    int ret = mbedtls_aes_setkey_dec(&aes, AES_KEY, 128);
    if (ret != 0) {
        DEBUG_PRINTF("[Crypto] ERRO ao configurar chave: %d\n", ret);
        mbedtls_aes_free(&aes);
        return false;
    }
    
    // Descriptografar bloco por bloco
    for (size_t i = 0; i < len; i += 16) {
        ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, 
                                     ciphertext + i, plaintext + i);
        if (ret != 0) {
            DEBUG_PRINTF("[Crypto] ERRO ao descriptografar bloco %d: %d\n", i/16, ret);
            mbedtls_aes_free(&aes);
            return false;
        }
    }
    
    mbedtls_aes_free(&aes);
    
    DEBUG_PRINTF("[Crypto] Descriptografado %d bytes\n", len);
    return true;
}

// ============================================================================
// Padding PKCS7 (para completar blocos de 16 bytes)
// ============================================================================
size_t CryptoManager::addPadding(const uint8_t* data, size_t len, uint8_t* paddedData) {
    // Copiar dados originais
    memcpy(paddedData, data, len);
    
    // Calcular quantos bytes faltam para completar bloco de 16
    uint8_t paddingBytes = 16 - (len % 16);
    
    // Adicionar padding (PKCS7: cada byte contém o número de bytes de padding)
    for (uint8_t i = 0; i < paddingBytes; i++) {
        paddedData[len + i] = paddingBytes;
    }
    
    size_t newLen = len + paddingBytes;
    
    DEBUG_PRINTF("[Crypto] Padding: %d -> %d bytes (adicionado %d)\n", 
                 len, newLen, paddingBytes);
    
    return newLen;
}

size_t CryptoManager::removePadding(const uint8_t* paddedData, size_t len) {
    if (len == 0 || len % 16 != 0) {
        DEBUG_PRINTLN("[Crypto] ERRO: Dados com padding inválidos.");
        return 0;
    }
    
    // O último byte indica quantos bytes de padding existem
    uint8_t paddingBytes = paddedData[len - 1];
    
    // Validar padding (deve ser entre 1 e 16)
    if (paddingBytes == 0 || paddingBytes > 16) {
        DEBUG_PRINTF("[Crypto] ERRO: Padding inválido (%d)\n", paddingBytes);
        return len; // Retorna tamanho original em caso de erro
    }
    
    // Verificar se todos os bytes de padding são iguais (PKCS7)
    for (uint8_t i = 1; i <= paddingBytes; i++) {
        if (paddedData[len - i] != paddingBytes) {
            DEBUG_PRINTLN("[Crypto] ERRO: Padding PKCS7 corrompido.");
            return len;
        }
    }
    
    size_t realLen = len - paddingBytes;
    
    DEBUG_PRINTF("[Crypto] Padding removido: %d -> %d bytes\n", len, realLen);
    
    return realLen;
}
