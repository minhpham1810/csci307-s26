#include "record.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "crypto_utils.h"

/* write exactly len bytes, looping on short writes */
static int send_all(int fd, const unsigned char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = (int)write(fd, buf + sent, (size_t)(len - sent));
        if (n <= 0) {
            return 0;
        }
        sent += n;
    }
    return 1;
}

/* read exactly len bytes, returns 0 if the connection closes early */
static int recv_all(int fd, unsigned char *buf, int len) {
    int received = 0;
    while (received < len) {
        int n = (int)read(fd, buf + received, (size_t)(len - received));
        if (n <= 0) {
            return 0;
        }
        received += n;
    }
    return 1;
}

int send_record(int fd, const SessionKeys *keys,
                const unsigned char *plaintext, int plaintext_len) {
    unsigned char iv[AES_IV_LEN];
    unsigned char ciphertext[RECORD_MAX_CIPHERTEXT];
    unsigned char hmac_input[AES_IV_LEN + RECORD_MAX_CIPHERTEXT];
    unsigned char mac[SHA256_DIGEST_LEN];
    uint32_t net_len;
    int ct_len;
    int total_payload_len;

    if (keys == NULL || plaintext == NULL || plaintext_len < 0) {
        return 0;
    }

    ct_len = aes256_cbc_encrypt(plaintext, plaintext_len,
                                keys->enc_key, iv,
                                ciphertext, RECORD_MAX_CIPHERTEXT);
    if (ct_len < 0) {
        return 0;
    }

    memcpy(hmac_input, iv, AES_IV_LEN);
    memcpy(hmac_input + AES_IV_LEN, ciphertext, ct_len);

    if (!compute_hmac_sha256(hmac_input, AES_IV_LEN + ct_len,
                             keys->hmac_key, mac)) {
        return 0;
    }

    total_payload_len = AES_IV_LEN + ct_len + SHA256_DIGEST_LEN;
    net_len = htonl((uint32_t)total_payload_len);

    if (!send_all(fd, (unsigned char *)&net_len, 4)) return 0;
    if (!send_all(fd, iv, AES_IV_LEN)) return 0;
    if (!send_all(fd, ciphertext, ct_len)) return 0;
    if (!send_all(fd, mac, SHA256_DIGEST_LEN)) return 0;

    return 1;
}

int recv_record(int fd, const SessionKeys *keys,
                unsigned char *plaintext_buf, int capacity) {
    uint32_t net_len;
    int total_payload_len;
    int ct_len;
    unsigned char frame[AES_IV_LEN + RECORD_MAX_CIPHERTEXT + SHA256_DIGEST_LEN];
    unsigned char expected_mac[SHA256_DIGEST_LEN];
    unsigned char *iv;
    unsigned char *ciphertext;
    unsigned char *received_mac;

    if (keys == NULL || plaintext_buf == NULL || capacity <= 0) {
        return -1;
    }

    if (!recv_all(fd, (unsigned char *)&net_len, 4)) {
        return -1;
    }

    total_payload_len = (int)ntohl(net_len);

    /* Sanity check bounds. */
    if (total_payload_len <= AES_IV_LEN + SHA256_DIGEST_LEN ||
        total_payload_len > AES_IV_LEN + RECORD_MAX_CIPHERTEXT + SHA256_DIGEST_LEN) {
        return -1;
    }

    ct_len = total_payload_len - AES_IV_LEN - SHA256_DIGEST_LEN;

    if (!recv_all(fd, frame, total_payload_len)) {
        return -1;
    }

    iv           = frame;
    ciphertext   = frame + AES_IV_LEN;
    received_mac = frame + AES_IV_LEN + ct_len;

    /* Verify HMAC over IV || ciphertext before decrypting. */
    if (!compute_hmac_sha256(frame, AES_IV_LEN + ct_len,
                             keys->hmac_key, expected_mac)) {
        return -1;
    }

    if (!secure_compare(expected_mac, received_mac, SHA256_DIGEST_LEN)) {
        return -1;
    }

    return aes256_cbc_decrypt(ciphertext, ct_len,
                              keys->enc_key, iv,
                              plaintext_buf, capacity);
}
