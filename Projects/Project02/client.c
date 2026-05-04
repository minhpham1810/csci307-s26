#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "common.h"
#include "crypto_utils.h"
#include "record.h"

/* ---------- helpers -------------------------------------------------- */

static int parse_command(const char *line, ClientCommand *cmd) {
    char verb[MAX_CMD_ARG_LEN];
    int matched;

    memset(cmd, 0, sizeof(ClientCommand));

    matched = sscanf(line, "%127s %127s %127s", verb, cmd->arg1, cmd->arg2);
    if (matched < 1) {
        return 0;
    }

    if (strcmp(verb, "req1") == 0) {
        cmd->type = CMD_REQ1;
    } else if (strcmp(verb, "req2") == 0) {
        cmd->type = CMD_REQ2;
    } else if (strcmp(verb, "adduser") == 0) {
        cmd->type = CMD_ADDUSER;
    } else if (strcmp(verb, "listusers") == 0) {
        cmd->type = CMD_LISTUSERS;
    } else if (strcmp(verb, "setrole") == 0) {
        cmd->type = CMD_SETROLE;
    } else if (strcmp(verb, "exit") == 0) {
        cmd->type = CMD_EXIT;
    } else {
        cmd->type = CMD_INVALID;
        return 0;
    }

    return 1;
}

/* ---------- protocol phases ------------------------------------------ */

static int do_handshake(int fd, SessionKeys *keys_out) {
    unsigned char pms[TLS_PMS_LEN];
    int sent = 0;

    if (!generate_pms(pms)) {
        return 0;
    }

    while (sent < TLS_PMS_LEN) {
        int n = (int)write(fd, pms + sent, (size_t)(TLS_PMS_LEN - sent));
        if (n <= 0) {
            return 0;
        }
        sent += n;
    }

    return derive_session_keys(pms, keys_out);
}

static int do_auth(int fd, const SessionKeys *keys,
                   const char *username, const char *password,
                   UserRole *role_out) {
    unsigned char buf[RECORD_MAX_PLAINTEXT];
    ClientRequest req;
    ServerChallenge *challenge;
    ClientResponse resp;
    AuthResult *result;
    unsigned char pw_hash[SHA256_DIGEST_LEN];
    int n;

    /* Send username. */
    memset(&req, 0, sizeof(req));
    strncpy(req.username, username, MAX_USERNAME_LEN - 1);
    req.username[MAX_USERNAME_LEN - 1] = '\0';
    if (!send_record(fd, keys, (unsigned char *)&req, (int)sizeof(req))) {
        return 0;
    }

    /* Receive challenge. */
    n = recv_record(fd, keys, buf, RECORD_MAX_PLAINTEXT);
    if (n != (int)sizeof(ServerChallenge)) {
        return 0;
    }
    challenge = (ServerChallenge *)buf;
    challenge->salt[SALT_HEX_LEN] = '\0';

    /* Compute response. */
    if (!hash_password_salted(password, challenge->salt, pw_hash)) {
        return 0;
    }
    memset(&resp, 0, sizeof(resp));
    if (!compute_response(challenge->nonce, pw_hash, resp.response)) {
        return 0;
    }
    if (!send_record(fd, keys, (unsigned char *)&resp, (int)sizeof(resp))) {
        return 0;
    }

    /* Receive auth result. */
    n = recv_record(fd, keys, buf, RECORD_MAX_PLAINTEXT);
    if (n != (int)sizeof(AuthResult)) {
        return 0;
    }
    result = (AuthResult *)buf;
    result->message[MAX_RESPONSE_LEN - 1] = '\0';

    if (!result->success) {
        fprintf(stderr, "Auth failed: %s\n", result->message);
        return 0;
    }

    printf("%s\n", result->message);
    *role_out = result->role;
    return 1;
}

static void do_command_loop(int fd, const SessionKeys *keys,
                            const char *username) {
    char line[256];
    ClientCommand cmd;
    unsigned char buf[RECORD_MAX_PLAINTEXT];
    ServerResponse *resp;
    int n;

    while (1) {
        printf("(%s) > ", username);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* EOF — send exit gracefully. */
            memset(&cmd, 0, sizeof(cmd));
            cmd.type = CMD_EXIT;
            send_record(fd, keys, (unsigned char *)&cmd, (int)sizeof(cmd));
            break;
        }

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') {
            continue;
        }

        if (!parse_command(line, &cmd)) {
            printf("Unknown command. Commands: req1 req2 adduser listusers setrole exit\n");
            continue;
        }

        if (!send_record(fd, keys, (unsigned char *)&cmd, (int)sizeof(cmd))) {
            break;
        }

        n = recv_record(fd, keys, buf, RECORD_MAX_PLAINTEXT);
        if (n != (int)sizeof(ServerResponse)) {
            break;
        }
        resp = (ServerResponse *)buf;
        resp->message[MAX_RESPONSE_LEN - 1] = '\0';
        printf("%s\n", resp->message);

        if (cmd.type == CMD_EXIT) {
            break;
        }
    }
}

/* ---------- main ----------------------------------------------------- */

int main(int argc, char *argv[]) {
    int fd;
    struct sockaddr_in addr;
    struct hostent *host;
    SessionKeys keys;
    UserRole role;

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <host> <port> <username> <password>\n", argv[0]);
        return 1;
    }

    host = gethostbyname(argv[1]);
    if (host == NULL) {
        fprintf(stderr, "Unknown host: %s\n", argv[1]);
        return 1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)atoi(argv[2]));
    memcpy(&addr.sin_addr, host->h_addr_list[0], (size_t)host->h_length);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); close(fd); return 1;
    }

    if (!do_handshake(fd, &keys)) {
        fprintf(stderr, "Handshake failed\n"); close(fd); return 1;
    }

    if (!do_auth(fd, &keys, argv[3], argv[4], &role)) {
        fprintf(stderr, "Authentication failed\n"); close(fd); return 1;
    }

    do_command_loop(fd, &keys, argv[3]);

    close(fd);
    return 0;
}
