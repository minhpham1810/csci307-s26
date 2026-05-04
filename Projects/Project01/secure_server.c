// secure_server.c - Account service server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

int main(int argc, char *argv[]) {
    int listen_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int opt = 1;
    int port = 12345;

    if (argc >= 2) port = atoi(argv[1]);

    // Create socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); close(listen_fd); return 1;
    }

    // Listen
    if (listen(listen_fd, 5) < 0) {
        perror("listen"); close(listen_fd); return 1;
    }

    printf("Server listening on port %d\n", port);

    // Accept loop
    while (1) {
        client_len = sizeof(client_addr);
        client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        printf("Client connected\n");

        // Receive request
        AccountRequest req;
        ssize_t n = recv(client_fd, &req, sizeof(req), 0);
        if (n != sizeof(req)) {
            fprintf(stderr, "Bad request size\n");
            close(client_fd);
            continue;
        }

        req.cmd_id = ntohl(req.cmd_id);
        req.account_id = ntohl(req.account_id);

        printf("Command: %d, Account: %d\n", req.cmd_id, req.account_id);

        if (req.cmd_id == CMD_BALANCE) {
            AccountInfo resp;
            resp.account_id = htonl(1001);
            resp.balance = 2500.75f;
            strncpy(resp.holder_name, "Minh Pham", sizeof(resp.holder_name));
            send(client_fd, &resp, sizeof(resp), 0);

        } else if (req.cmd_id == CMD_TRANSACTION) {
            TransactionResult resp;
            resp.account_id = htonl(1001);
            resp.status = htonl(0);
            resp.new_balance = 2550.75f;
            send(client_fd, &resp, sizeof(resp), 0);

        } else {
            fprintf(stderr, "Unknown command: %d\n", req.cmd_id);
        }

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}
