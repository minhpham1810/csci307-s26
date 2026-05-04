#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <stddef.h>

#include "common.h"

int hash_password_salted(const char *password,
                         const char *salt,
                         unsigned char hash_output[SHA256_DIGEST_LEN]);

int compute_response(const unsigned char nonce[NONCE_LEN],
                     const unsigned char password_hash[SHA256_DIGEST_LEN],
                     unsigned char response_output[SHA256_DIGEST_LEN]);

int generate_nonce(unsigned char *nonce_buffer, int len);

int generate_salt(char *salt_buffer, int len);

void bytes_to_hex(const unsigned char *bytes, int len, char *hex_output);

int hex_to_bytes(const char *hex, unsigned char *bytes, int max_len);

void print_hex(const char *label, const unsigned char *bytes, int len);

int generate_pms(unsigned char pms[TLS_PMS_LEN]);

int derive_session_keys(const unsigned char pms[TLS_PMS_LEN],
                        SessionKeys *keys);

int aes256_cbc_encrypt(const unsigned char *plaintext,
                       int plaintext_len,
                       const unsigned char key[AES_KEY_LEN],
                       unsigned char iv[AES_IV_LEN],
                       unsigned char *ciphertext,
                       int ciphertext_capacity);

int aes256_cbc_decrypt(const unsigned char *ciphertext,
                       int ciphertext_len,
                       const unsigned char key[AES_KEY_LEN],
                       const unsigned char iv[AES_IV_LEN],
                       unsigned char *plaintext,
                       int plaintext_capacity);

int compute_hmac_sha256(const unsigned char *data,
                        int data_len,
                        const unsigned char key[HMAC_KEY_LEN],
                        unsigned char mac_output[SHA256_DIGEST_LEN]);

int secure_compare(const unsigned char *a,
                   const unsigned char *b,
                   size_t len);

#endif