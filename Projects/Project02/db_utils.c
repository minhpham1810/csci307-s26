#include "db_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crypto_utils.h"

static UserRecord *alloc_record(void) {
    UserRecord *r = malloc(sizeof(UserRecord));
    if (r != NULL) {
        memset(r, 0, sizeof(UserRecord));
    }
    return r;
}

UserRecord *load_database(void) {
    FILE *fp;
    char line[512];
    UserRecord *head = NULL;
    UserRecord *tail = NULL;

    fp = fopen(USER_DB_FILE, "r");
    if (fp == NULL) {
        return NULL;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char uname[MAX_USERNAME_LEN];
        char role_str[MAX_ROLE_LEN];
        char salt[SALT_HEX_LEN + 2];
        char hex_hash[SHA256_DIGEST_LEN * 2 + 2];
        UserRecord *node;

        /* Strip trailing newline. */
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') {
            continue;
        }

        if (sscanf(line, "%31[^:]:%15[^:]:%17[^:]:%64s",
                   uname, role_str, salt, hex_hash) != 4) {
            continue;
        }

        node = alloc_record();
        if (node == NULL) {
            break;
        }

        strncpy(node->username, uname, MAX_USERNAME_LEN - 1);
        node->username[MAX_USERNAME_LEN - 1] = '\0';

        strncpy(node->salt, salt, SALT_HEX_LEN);
        node->salt[SALT_HEX_LEN] = '\0';

        if (!string_to_role(role_str, &node->role)) {
            free(node);
            continue;
        }

        if (hex_to_bytes(hex_hash, node->stored_hash, SHA256_DIGEST_LEN) != SHA256_DIGEST_LEN) {
            free(node);
            continue;
        }

        node->next = NULL;
        if (head == NULL) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
    }

    fclose(fp);
    return head;
}

int save_database(UserRecord *head) {
    FILE *fp;
    UserRecord *cur;
    char hex_hash[SHA256_DIGEST_LEN * 2 + 1];

    fp = fopen(USER_DB_FILE, "w");
    if (fp == NULL) {
        return 0;
    }

    for (cur = head; cur != NULL; cur = cur->next) {
        bytes_to_hex(cur->stored_hash, SHA256_DIGEST_LEN, hex_hash);
        fprintf(fp, "%s:%s:%s:%s\n",
                cur->username,
                role_to_string(cur->role),
                cur->salt,
                hex_hash);
    }

    fclose(fp);
    return 1;
}

UserRecord *find_user(UserRecord *head, const char *username) {
    UserRecord *cur;

    if (username == NULL) {
        return NULL;
    }

    for (cur = head; cur != NULL; cur = cur->next) {
        if (strncmp(cur->username, username, MAX_USERNAME_LEN - 1) == 0) {
            return cur;
        }
    }
    return NULL;
}

int add_user(UserRecord **head_ptr, const char *username,
             UserRole role, const char *password) {
    UserRecord *node;
    UserRecord *cur;

    if (head_ptr == NULL || username == NULL || password == NULL) {
        return 0;
    }

    if (strnlen(username, MAX_USERNAME_LEN) >= MAX_USERNAME_LEN - 1) {
        return 0;
    }

    if (find_user(*head_ptr, username) != NULL) {
        return 0;
    }

    node = alloc_record();
    if (node == NULL) {
        return 0;
    }

    strncpy(node->username, username, MAX_USERNAME_LEN - 1);
    node->username[MAX_USERNAME_LEN - 1] = '\0';
    node->role = role;

    if (!generate_salt(node->salt, SALT_HEX_LEN)) {
        free(node);
        return 0;
    }
    node->salt[SALT_HEX_LEN] = '\0';

    if (!hash_password_salted(password, node->salt, node->stored_hash)) {
        free(node);
        return 0;
    }

    node->next = NULL;

    if (*head_ptr == NULL) {
        *head_ptr = node;
        return 1;
    }

    for (cur = *head_ptr; cur->next != NULL; cur = cur->next)
        ;
    cur->next = node;
    return 1;
}

int set_user_role(UserRecord *head, const char *username, UserRole role) {
    UserRecord *node = find_user(head, username);
    if (node == NULL) {
        return 0;
    }
    node->role = role;
    return 1;
}

void free_database(UserRecord *head) {
    UserRecord *cur = head;
    while (cur != NULL) {
        UserRecord *next = cur->next;
        free(cur);
        cur = next;
    }
}
