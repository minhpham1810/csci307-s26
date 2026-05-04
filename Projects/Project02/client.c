#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include "common.h"
#include "crypto_utils.h"
#include "record.h"

static int write_all(int fd, const unsigned char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = (int)write(fd, buf + sent, (size_t)(len - sent));
        if (n <= 0) return 0;
        sent += n;
    }
    return 1;
}

static int read_all(int fd, unsigned char *buf, int len) {
    int got = 0;
    while (got < len) {
        int n = (int)read(fd, buf + got, (size_t)(len - got));
        if (n <= 0) return 0;
        got += n;
    }
    return 1;
}

/* parse a line from stdin into a ClientCommand struct, returns 0 on unknown verb */
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

/*
 * TLS-style handshake:
 * 1. Receive server's RSA public key (SPKI DER)
 * 2. Generate a random 48-byte PMS
 * 3. Encrypt PMS with server's public key (OAEP) and send it
 * 4. Derive session keys from the PMS
 */
static int do_handshake(int fd, SessionKeys *keys_out) {
    uint32_t net_len;
    int pub_len;
    unsigned char pub_der[1024]; /* enough for a 2048-bit RSA SPKI */
    const unsigned char *p;
    EVP_PKEY *server_pub = NULL;
    unsigned char pms[TLS_PMS_LEN];
    unsigned char enc_pms[512];
    size_t enc_len = sizeof(enc_pms);
    EVP_PKEY_CTX *enc_ctx = NULL;
    uint32_t enc_net_len;
    int ret = 0;

    /* receive server's public key */
    if (!read_all(fd, (unsigned char *)&net_len, 4)) return 0;
    pub_len = (int)ntohl(net_len);
    if (pub_len <= 0 || pub_len > (int)sizeof(pub_der)) return 0;
    if (!read_all(fd, pub_der, pub_len)) return 0;

    p = pub_der;
    server_pub = d2i_PUBKEY(NULL, &p, pub_len);
    if (server_pub == NULL) return 0;

    /* generate PMS and encrypt with server's public key */
    if (!generate_pms(pms)) {
        EVP_PKEY_free(server_pub);
        return 0;
    }

    enc_ctx = EVP_PKEY_CTX_new(server_pub, NULL);
    if (enc_ctx == NULL) {
        EVP_PKEY_free(server_pub);
        return 0;
    }

    if (EVP_PKEY_encrypt_init(enc_ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(enc_ctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_encrypt(enc_ctx, enc_pms, &enc_len, pms, TLS_PMS_LEN) <= 0) {
        EVP_PKEY_CTX_free(enc_ctx);
        EVP_PKEY_free(server_pub);
        return 0;
    }
    EVP_PKEY_CTX_free(enc_ctx);
    EVP_PKEY_free(server_pub);

    /* send encrypted PMS */
    enc_net_len = htonl((uint32_t)enc_len);
    if (!write_all(fd, (unsigned char *)&enc_net_len, 4) ||
        !write_all(fd, enc_pms, (int)enc_len)) {
        memset(pms, 0, sizeof(pms));
        return 0;
    }

    ret = derive_session_keys(pms, keys_out);
    memset(pms, 0, sizeof(pms)); /* clear PMS from stack */
    return ret;
}

/*
 * Run the challenge-response auth protocol:
 *   1. send username
 *   2. receive nonce + salt from server
 *   3. compute response = SHA-256(nonce || SHA-256(salt || password))
 *   4. send response, receive auth result
 */
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

/* interactive command loop - read from stdin, send to server, print reply */
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
            /* EOF - send exit so the server closes cleanly */
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
