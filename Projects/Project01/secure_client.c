// secure_client.c - Account service client

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = atoi(argv[2]);

    // Request 1: check balance
    int sockfd = create_connection(host, port);
    if (sockfd < 0) return 1;

    AccountRequest req;
    req.cmd_id = htonl(CMD_BALANCE);
    req.account_id = htonl(1001);
    req.auth_token = htonl(1111);
    req.amount = 0;

    send(sockfd, &req, sizeof(req), 0);

    AccountInfo info;
    recv(sockfd, &info, sizeof(info), 0);
    info.account_id = ntohl(info.account_id);

    printf("Account: %d, Name: %s, Balance: %.2f\n",
           info.account_id, info.holder_name, info.balance);

    close(sockfd);

    // Request 2: make transaction (separate connection)
    sockfd = create_connection(host, port);
    if (sockfd < 0) return 1;

    req.cmd_id = htonl(CMD_TRANSACTION);
    req.account_id = htonl(1001);
    req.auth_token = htonl(1111);
    req.amount = 50.0f;

    send(sockfd, &req, sizeof(req), 0);

    TransactionResult result;
    recv(sockfd, &result, sizeof(result), 0);
    result.account_id = ntohl(result.account_id);
    result.status = ntohl(result.status);

    printf("Transaction: account %d, status %d, new balance: %.2f\n",
           result.account_id, result.status, result.new_balance);

    close(sockfd);

    return 0;
}
