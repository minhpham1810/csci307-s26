#include "crypto_utils.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>

int hash_password_salted(const char *password,
                         const char *salt,
                         unsigned char hash_output[SHA256_DIGEST_LEN]) {
    EVP_MD_CTX *ctx = NULL;
    unsigned int out_len = 0;

    if (password == NULL || salt == NULL || hash_output == NULL) {
        return 0;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return 0;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, salt, strlen(salt)) != 1 ||
        EVP_DigestUpdate(ctx, password, strlen(password)) != 1 ||
        EVP_DigestFinal_ex(ctx, hash_output, &out_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }

    EVP_MD_CTX_free(ctx);
    return out_len == SHA256_DIGEST_LEN;
}

int compute_response(const unsigned char nonce[NONCE_LEN],
                     const unsigned char password_hash[SHA256_DIGEST_LEN],
                     unsigned char response_output[SHA256_DIGEST_LEN]) {
    EVP_MD_CTX *ctx = NULL;
    unsigned int out_len = 0;

    if (nonce == NULL || password_hash == NULL || response_output == NULL) {
        return 0;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return 0;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, nonce, NONCE_LEN) != 1 ||
        EVP_DigestUpdate(ctx, password_hash, SHA256_DIGEST_LEN) != 1 ||
        EVP_DigestFinal_ex(ctx, response_output, &out_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }

    EVP_MD_CTX_free(ctx);
    return out_len == SHA256_DIGEST_LEN;
}

int generate_nonce(unsigned char *nonce_buffer, int len) {
    if (nonce_buffer == NULL || len <= 0) {
        return 0;
    }

    return RAND_bytes(nonce_buffer, len) == 1;
}

int generate_salt(char *salt_buffer, int len) {
    unsigned char random_bytes[SALT_HEX_LEN / 2 + 1];
    int byte_len;

    if (salt_buffer == NULL || len <= 0) {
        return 0;
    }

    if (len % 2 != 0) {
        return 0;
    }

    byte_len = len / 2;
    if (byte_len > (int)sizeof(random_bytes)) {
        return 0;
    }

    if (RAND_bytes(random_bytes, byte_len) != 1) {
        return 0;
    }

    bytes_to_hex(random_bytes, byte_len, salt_buffer);
    salt_buffer[len] = '\0';
    return 1;
}

void bytes_to_hex(const unsigned char *bytes, int len, char *hex_output) {
    static const char hex_chars[] = "0123456789abcdef";
    int i;

    if (bytes == NULL || hex_output == NULL || len < 0) {
        return;
    }

    for (i = 0; i < len; i++) {
        hex_output[i * 2] = hex_chars[(bytes[i] >> 4) & 0x0F];
        hex_output[i * 2 + 1] = hex_chars[bytes[i] & 0x0F];
    }

    hex_output[len * 2] = '\0';
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

int hex_to_bytes(const char *hex, unsigned char *bytes, int max_len) {
    int hex_len;
    int out_len;
    int i;

    if (hex == NULL || bytes == NULL || max_len <= 0) {
        return -1;
    }

    hex_len = (int)strlen(hex);
    if (hex_len % 2 != 0) {
        return -1;
    }

    out_len = hex_len / 2;
    if (out_len > max_len) {
        return -1;
    }

    for (i = 0; i < out_len; i++) {
        int high = hex_value(hex[i * 2]);
        int low = hex_value(hex[i * 2 + 1]);

        if (high < 0 || low < 0) {
            return -1;
        }

        bytes[i] = (unsigned char)((high << 4) | low);
    }

    return out_len;
}

void print_hex(const char *label, const unsigned char *bytes, int len) {
    int i;

    if (label != NULL) {
        printf("%s", label);
    }

    if (bytes == NULL || len < 0) {
        printf("(null)\n");
        return;
    }

    for (i = 0; i < len; i++) {
        printf("%02x", bytes[i]);
    }

    printf("\n");
}

int generate_pms(unsigned char pms[TLS_PMS_LEN]) {
    if (pms == NULL) {
        return 0;
    }

    return RAND_bytes(pms, TLS_PMS_LEN) == 1;
}

static int sha256_labeled(const char *label,
                          const unsigned char *input,
                          int input_len,
                          unsigned char out[SHA256_DIGEST_LEN]) {
    EVP_MD_CTX *ctx = NULL;
    unsigned int out_len = 0;

    if (label == NULL || input == NULL || input_len < 0 || out == NULL) {
        return 0;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return 0;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, label, strlen(label)) != 1 ||
        EVP_DigestUpdate(ctx, input, input_len) != 1 ||
        EVP_DigestFinal_ex(ctx, out, &out_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return 0;
    }

    EVP_MD_CTX_free(ctx);
    return out_len == SHA256_DIGEST_LEN;
}

int derive_session_keys(const unsigned char pms[TLS_PMS_LEN],
                        SessionKeys *keys) {
    if (pms == NULL || keys == NULL) {
        return 0;
    }

    if (!sha256_labeled("enc", pms, TLS_PMS_LEN, keys->enc_key)) {
        return 0;
    }

    if (!sha256_labeled("hmac", pms, TLS_PMS_LEN, keys->hmac_key)) {
        return 0;
    }

    return 1;
}

int aes256_cbc_encrypt(const unsigned char *plaintext,
                       int plaintext_len,
                       const unsigned char key[AES_KEY_LEN],
                       unsigned char iv[AES_IV_LEN],
                       unsigned char *ciphertext,
                       int ciphertext_capacity) {
    EVP_CIPHER_CTX *ctx = NULL;
    int len = 0;
    int ciphertext_len = 0;

    if (plaintext == NULL || plaintext_len < 0 ||
        key == NULL || iv == NULL ||
        ciphertext == NULL || ciphertext_capacity <= 0) {
        return -1;
    }

    if (RAND_bytes(iv, AES_IV_LEN) != 1) {
        return -1;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        return -1;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (ciphertext_capacity < plaintext_len + AES_IV_LEN) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_EncryptUpdate(ctx,
                          ciphertext,
                          &len,
                          plaintext,
                          plaintext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    ciphertext_len = len;

    if (EVP_EncryptFinal_ex(ctx,
                            ciphertext + ciphertext_len,
                            &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

int aes256_cbc_decrypt(const unsigned char *ciphertext,
                       int ciphertext_len,
                       const unsigned char key[AES_KEY_LEN],
                       const unsigned char iv[AES_IV_LEN],
                       unsigned char *plaintext,
                       int plaintext_capacity) {
    EVP_CIPHER_CTX *ctx = NULL;
    int len = 0;
    int plaintext_len = 0;

    if (ciphertext == NULL || ciphertext_len <= 0 ||
        key == NULL || iv == NULL ||
        plaintext == NULL || plaintext_capacity <= 0) {
        return -1;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        return -1;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (plaintext_capacity < ciphertext_len) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_DecryptUpdate(ctx,
                          plaintext,
                          &len,
                          ciphertext,
                          ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    plaintext_len = len;

    if (EVP_DecryptFinal_ex(ctx,
                            plaintext + plaintext_len,
                            &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    plaintext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return plaintext_len;
}

int compute_hmac_sha256(const unsigned char *data,
                        int data_len,
                        const unsigned char key[HMAC_KEY_LEN],
                        unsigned char mac_output[SHA256_DIGEST_LEN]) {
    unsigned int mac_len = 0;

    if (data == NULL || data_len < 0 || key == NULL || mac_output == NULL) {
        return 0;
    }

    if (HMAC(EVP_sha256(),
             key,
             HMAC_KEY_LEN,
             data,
             data_len,
             mac_output,
             &mac_len) == NULL) {
        return 0;
    }

    return mac_len == SHA256_DIGEST_LEN;
}

int secure_compare(const unsigned char *a,
                   const unsigned char *b,
                   size_t len) {
    if (a == NULL || b == NULL) {
        return 0;
    }

    return CRYPTO_memcmp(a, b, len) == 0;
}