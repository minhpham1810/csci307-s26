#ifndef DB_UTILS_H
#define DB_UTILS_H

#include "common.h"

/* one node per user in the linked list */
typedef struct UserRecord {
    char username[MAX_USERNAME_LEN];
    UserRole role;
    char salt[SALT_HEX_LEN + 1];
    unsigned char stored_hash[SHA256_DIGEST_LEN]; /* SHA-256(salt || password) */
    struct UserRecord *next;
} UserRecord;

/* parse user_db.txt into a linked list, returns NULL if file missing */
UserRecord *load_database(void);

/* write the whole list back to user_db.txt, call after any change */
int save_database(UserRecord *head);

/* find a user by username, returns NULL if not found */
UserRecord *find_user(UserRecord *head, const char *username);

/* create a new user with a fresh salt+hash and append to list */
int add_user(UserRecord **head_ptr, const char *username,
             UserRole role, const char *password);

/* change a user's role in place, returns 0 if user doesn't exist */
int set_user_role(UserRecord *head, const char *username, UserRole role);

/* free the whole list */
void free_database(UserRecord *head);

#endif
