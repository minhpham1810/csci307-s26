// protocol.h - Shared protocol definitions

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define CMD_BALANCE     1
#define CMD_TRANSACTION 2

// Request sent by client
typedef struct {
    int32_t cmd_id;
    int32_t account_id;
    int32_t auth_token;
    float   amount;
} AccountRequest;

// Response for cmd_id = 1
typedef struct {
    int32_t account_id;
    float   balance;
    char    holder_name[32];
} AccountInfo;

// Response for cmd_id = 2
typedef struct {
    int32_t account_id;
    int32_t status;
    float   new_balance;
} TransactionResult;

#endif
