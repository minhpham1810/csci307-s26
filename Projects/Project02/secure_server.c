// secure_server.c - Account service server

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

static int parse_port(const char *text, int *port_out) {
    char *end = NULL;
    long port;

    port = strtol(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || port < 1 || port > 65535) {
        return -1;
    }

    *port_out = (int)port;
    return 0;
}

static int send_balance_response(int client_fd) {
    AccountInfo resp;

    memset(&resp, 0, sizeof(resp));
    resp.account_id = htonl(1001);
    resp.balance = 2500.75f;
    strncpy(resp.holder_name, "Minh Pham", sizeof(resp.holder_name) - 1);
    resp.holder_name[sizeof(resp.holder_name) - 1] = '\0';

    return send(client_fd, &resp, sizeof(resp), 0) == (ssize_t)sizeof(resp) ? 0 : -1;
}

static int send_transaction_response(int client_fd) {
    TransactionResult resp;

    memset(&resp, 0, sizeof(resp));
    resp.account_id = htonl(1001);
    resp.status = htonl(0);
    resp.new_balance = 2550.75f;

    return send(client_fd, &resp, sizeof(resp), 0) == (ssize_t)sizeof(resp) ? 0 : -1;
}

static void handle_client(int client_fd) {
    AccountRequest req;
    ssize_t n;

    n = recv(client_fd, &req, sizeof(req), 0);
    if (n != (ssize_t)sizeof(req)) {
        fprintf(stderr, "Bad request size\n");
        return;
    }

    req.cmd_id = ntohl(req.cmd_id);
    req.account_id = ntohl(req.account_id);

    printf("Command: %d, Account: %d\n", req.cmd_id, req.account_id);

    if (req.cmd_id == CMD_BALANCE) {
        if (send_balance_response(client_fd) < 0) {
            perror("send");
        }
    } else if (req.cmd_id == CMD_TRANSACTION) {
        if (send_transaction_response(client_fd) < 0) {
            perror("send");
        }
    } else {
        fprintf(stderr, "Unknown command: %d\n", req.cmd_id);
    }
}

int main(int argc, char *argv[]) {
    int listen_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int opt = 1;
    int port = 12345;

    if (argc >= 2 && parse_port(argv[1], &port) != 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

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

        handle_client(client_fd);

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}
