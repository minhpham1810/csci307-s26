gcc#include <stdio.h>
#include "db_utils.h"

int main(void) {
    UserRecord *head = NULL;

    if (!add_user(&head, "admin", ROLE_ADMIN, "password123")) {
        fprintf(stderr, "Failed to add admin\n");
        return 1;
    }

    if (!add_user(&head, "alice", ROLE_USER, "password123")) {
        fprintf(stderr, "Failed to add alice\n");
        free_database(head);
        return 1;
    }

    if (!save_database(head)) {
        fprintf(stderr, "Failed to save database\n");
        free_database(head);
        return 1;
    }

    printf("Created %s with users: admin (ADMIN), alice (USER)\n", USER_DB_FILE);
    free_database(head);
    return 0;
}
