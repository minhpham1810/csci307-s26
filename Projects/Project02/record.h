#ifndef RECORD_H
#define RECORD_H

#include "common.h"

/*
 * Wire format: [4-byte len][16-byte IV][ciphertext][32-byte HMAC(IV||ciphertext)]
 * HMAC is over the IV+ciphertext so tampering is caught before we decrypt.
 */

/* encrypt and send one message, returns 1 on success */
int send_record(int fd, const SessionKeys *keys,
                const unsigned char *plaintext, int plaintext_len);

/* receive and decrypt one message, returns plaintext length or -1 on error */
int recv_record(int fd, const SessionKeys *keys,
                unsigned char *plaintext_buf, int capacity);

#endif
