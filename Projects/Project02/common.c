#include "common.h"

#include <string.h>

const char *role_to_string(UserRole role) {
    switch (role) {
        case ROLE_ADMIN:
            return "ADMIN";
        case ROLE_USER:
            return "USER";
        default:
            return "UNKNOWN";
    }
}

int string_to_role(const char *s, UserRole *out) {
    if (s == NULL || out == NULL) {
        return 0;
    }

    if (strcmp(s, "ADMIN") == 0) {
        *out = ROLE_ADMIN;
        return 1;
    }

    if (strcmp(s, "USER") == 0) {
        *out = ROLE_USER;
        return 1;
    }

    return 0;
}