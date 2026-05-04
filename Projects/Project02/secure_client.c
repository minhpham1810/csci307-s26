// secure_client.c - Account service client

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

int create_connection(const char *host, int port);

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

static int request_balance(const char *host, int port, int account_id, int auth_token) {
    int sockfd;
    AccountRequest req;
    AccountInfo info;
    ssize_t n;

    sockfd = create_connection(host, port);
    if (sockfd < 0) {
        return 1;
    }

    req.cmd_id = htonl(CMD_BALANCE);
    req.account_id = htonl(account_id);
    req.auth_token = htonl(auth_token);
    req.amount = 0;

    send(sockfd, &req, sizeof(req), 0);

    n = recv(sockfd, &info, sizeof(info), 0);
    if (n != (ssize_t)sizeof(info)) {
        fprintf(stderr, "Integrity error: recv AccountInfo got %zd, expected %zu\n",
                n, sizeof(info));
        close(sockfd);
        return 1;
    }

    info.account_id = ntohl(info.account_id);

    printf("Account: %d, Name: %s, Balance: %.2f\n",
           info.account_id, info.holder_name, info.balance);

    close(sockfd);
    return 0;
}

static int request_transaction(const char *host, int port, int account_id,
                               int auth_token, float amount) {
    int sockfd;
    AccountRequest req;
    TransactionResult result;
    ssize_t n;

    sockfd = create_connection(host, port);
    if (sockfd < 0) {
        return 1;
    }

    req.cmd_id = htonl(CMD_TRANSACTION);
    req.account_id = htonl(account_id);
    req.auth_token = htonl(auth_token);
    req.amount = amount;

    send(sockfd, &req, sizeof(req), 0);

    n = recv(sockfd, &result, sizeof(result), 0);
    if (n != (ssize_t)sizeof(result)) {
        fprintf(stderr, "Integrity error: recv TransactionResult got %zd, expected %zu\n",
                n, sizeof(result));
        close(sockfd);
        return 1;
    }

    result.account_id = ntohl(result.account_id);
    result.status = ntohl(result.status);

    printf("Transaction: account %d, status %d, new balance: %.2f\n",
           result.account_id, result.status, result.new_balance);

    close(sockfd);
    return 0;
}

int create_connection(const char *host, int port) {
    int sockfd;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return -1; }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(sockfd); return -1;
    }

    return sockfd;
}

int main(int argc, char *argv[]) {
    const char *host;
    int port;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }

    host = argv[1];
    if (parse_port(argv[2], &port) != 0) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 1;
    }

    if (request_balance(host, port, 1001, 1111) != 0) {
        return 1;
    }

    if (request_transaction(host, port, 1001, 1111, 50.0f) != 0) {
        return 1;
    }

    return 0;
}
