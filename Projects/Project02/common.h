#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

#define MAX_USERNAME_LEN 32
#define MAX_PASSWORD_LEN 128
#define MAX_ROLE_LEN 16

#define SALT_HEX_LEN 16
#define SHA256_DIGEST_LEN 32
#define NONCE_LEN 32

#define MAX_CMD_ARG_LEN 128
#define MAX_RESPONSE_LEN 1024

#define TLS_PMS_LEN 48
#define AES_KEY_LEN 32
#define HMAC_KEY_LEN 32
#define AES_IV_LEN 16

#define RECORD_MAX_PLAINTEXT 2048
#define RECORD_MAX_CIPHERTEXT 4096

#define USER_DB_FILE "user_db.txt"

typedef enum {
    ROLE_USER = 0,
    ROLE_ADMIN = 1
} UserRole;


typedef enum {
    CMD_REQ1 = 1,
    CMD_REQ2 = 2,
    CMD_ADDUSER = 3,
    CMD_LISTUSERS = 4,
    CMD_SETROLE = 5,
    CMD_EXIT = 6,
    CMD_INVALID = 99
} CommandType;


typedef struct {
    char username[MAX_USERNAME_LEN];
} ClientRequest;

typedef struct {
    unsigned char nonce[NONCE_LEN];
    char salt[SALT_HEX_LEN + 1];
} ServerChallenge;

typedef struct {
    unsigned char response[SHA256_DIGEST_LEN];
} ClientResponse;

typedef struct {
    int success;
    UserRole role;
    char message[MAX_RESPONSE_LEN];
} AuthResult;

typedef struct {
    CommandType type;
    char arg1[MAX_CMD_ARG_LEN];
    char arg2[MAX_CMD_ARG_LEN];
} ClientCommand;

typedef struct {
    int success;
    char message[MAX_RESPONSE_LEN];
} ServerResponse;

typedef struct {
    unsigned char enc_key[AES_KEY_LEN];
    unsigned char hmac_key[HMAC_KEY_LEN];
} SessionKeys;

const char *role_to_string(UserRole role);
int string_to_role(const char *s, UserRole *out);

#endif