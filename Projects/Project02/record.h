#ifndef RECORD_H
#define RECORD_H

#include "common.h"

/*
 * Wire format after handshake:
 * [4-byte total_payload_len][16-byte IV][ciphertext][32-byte HMAC(IV||ciphertext)]
 * HMAC is Encrypt-then-MAC over (IV || ciphertext).
 */

int send_record(int fd, const SessionKeys *keys,
                const unsigned char *plaintext, int plaintext_len);

int recv_record(int fd, const SessionKeys *keys,
                unsigned char *plaintext_buf, int capacity);

#endif
