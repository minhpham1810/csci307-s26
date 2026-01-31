#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* get_secure_input(size_t max_len) {
    char *buffer = malloc(max_len);
    if (buffer == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    if (fgets(buffer, max_len, stdin) == NULL) {
        buffer[0] = '\0';
    }

    buffer[max_len - 1] = '\0';

    return buffer;
}

int main(void) {
    char *input = get_secure_input(16);

    printf("%s\n", input);

    free(input);
    return 0;
}
