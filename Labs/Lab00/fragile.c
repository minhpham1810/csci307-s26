#include <stdio.h>
#include <string.h>

struct User {
    char username[16];
    char secret_key[16];
};

int main(void) {
    struct User user;

    strcpy(user.secret_key, "SUPER_SECRET");

    //scanf("%s", user.username);

    fgets(user.username, sizeof(user.username), stdin);

    printf("%s\n", user.secret_key);

    return 0;
}
