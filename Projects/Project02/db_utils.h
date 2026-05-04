#ifndef DB_UTILS_H
#define DB_UTILS_H

#include "common.h"

typedef struct UserRecord {
    char username[MAX_USERNAME_LEN];
    UserRole role;
    char salt[SALT_HEX_LEN + 1];
    unsigned char stored_hash[SHA256_DIGEST_LEN];
    struct UserRecord *next;
} UserRecord;

UserRecord *load_database(void);
int         save_database(UserRecord *head);
UserRecord *find_user(UserRecord *head, const char *username);
int         add_user(UserRecord **head_ptr, const char *username,
                     UserRole role, const char *password);
int         set_user_role(UserRecord *head, const char *username, UserRole role);
void        free_database(UserRecord *head);

#endif
