#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include "common.h"
#include "crypto_utils.h"
#include "db_utils.h"
#include "record.h"
#include "protocol.h"

/* user list loaded at startup, kept in memory for the server's lifetime */
static UserRecord *g_db_head = NULL;

/* RSA-2048 keypair generated once at startup, used for the PMS handshake */
static EVP_PKEY *g_server_key = NULL;


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

/* send an encrypted failure response to the client */
static int send_error(int fd, const SessionKeys *keys, const char *msg) {
    ServerResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.success = 0;
    strncpy(resp.message, msg, MAX_RESPONSE_LEN - 1);
    resp.message[MAX_RESPONSE_LEN - 1] = '\0';
    return send_record(fd, keys, (unsigned char *)&resp, (int)sizeof(resp));
}

/* send an encrypted success response to the client */
static int send_ok(int fd, const SessionKeys *keys, const char *msg) {
    ServerResponse resp;
    memset(&resp, 0, sizeof(resp));
    resp.success = 1;
    strncpy(resp.message, msg, MAX_RESPONSE_LEN - 1);
    resp.message[MAX_RESPONSE_LEN - 1] = '\0';
    return send_record(fd, keys, (unsigned char *)&resp, (int)sizeof(resp));
}

/* req1 - return hardcoded account info (project 1 behavior) */
static int cmd_req1(int fd, const SessionKeys *keys) {
    char buf[MAX_RESPONSE_LEN];
    AccountInfo info;
    memset(&info, 0, sizeof(info));
    info.account_id = 1001;
    info.balance = 500.00f;
    strncpy(info.holder_name, "Alice", sizeof(info.holder_name) - 1);
    info.holder_name[sizeof(info.holder_name) - 1] = '\0';
    snprintf(buf, sizeof(buf), "Account: %d | Holder: %s | Balance: %.2f",
             info.account_id, info.holder_name, info.balance);
    return send_ok(fd, keys, buf);
}

/* req2 - return hardcoded transaction result (project 1 behavior) */
static int cmd_req2(int fd, const SessionKeys *keys) {
    char buf[MAX_RESPONSE_LEN];
    TransactionResult tr;
    memset(&tr, 0, sizeof(tr));
    tr.account_id = 1001;
    tr.status = 1;
    tr.new_balance = 450.00f;
    snprintf(buf, sizeof(buf), "Transaction: account %d | status %d | new balance: %.2f",
             tr.account_id, tr.status, tr.new_balance);
    return send_ok(fd, keys, buf);
}

/* adduser - create a new USER-role account and save to disk */
static int cmd_adduser(int fd, const SessionKeys *keys, const ClientCommand *cmd) {
    if (cmd->arg1[0] == '\0' || cmd->arg2[0] == '\0') {
        return send_error(fd, keys, "Usage: adduser <username> <password>");
    }
    if (!add_user(&g_db_head, cmd->arg1, ROLE_USER, cmd->arg2)) {
        return send_error(fd, keys, "ERROR: User already exists or invalid input.");
    }
    if (!save_database(g_db_head)) {
        return send_error(fd, keys, "ERROR: Failed to save database.");
    }
    return send_ok(fd, keys, "User added successfully.");
}

/* listusers - send back all usernames and roles (no hashes or salts) */
static int cmd_listusers(int fd, const SessionKeys *keys) {
    char buf[MAX_RESPONSE_LEN];
    int offset = 0;
    UserRecord *cur;

    buf[0] = '\0';
    for (cur = g_db_head; cur != NULL; cur = cur->next) {
        int written = snprintf(buf + offset, sizeof(buf) - (size_t)offset,
                               "%s (%s)\n", cur->username, role_to_string(cur->role));
        if (written < 0 || offset + written >= (int)sizeof(buf)) {
            break;
        }
        offset += written;
    }

    if (offset == 0) {
        return send_ok(fd, keys, "(no users)");
    }
    return send_ok(fd, keys, buf);
}

/* setrole - change a user's role and persist to disk */
static int cmd_setrole(int fd, const SessionKeys *keys, const ClientCommand *cmd) {
    UserRole new_role;
    if (cmd->arg1[0] == '\0' || cmd->arg2[0] == '\0') {
        return send_error(fd, keys, "Usage: setrole <username> <USER|ADMIN>");
    }
    if (!string_to_role(cmd->arg2, &new_role)) {
        return send_error(fd, keys, "ERROR: Role must be USER or ADMIN.");
    }
    if (!set_user_role(g_db_head, cmd->arg1, new_role)) {
        return send_error(fd, keys, "ERROR: User not found.");
    }
    if (!save_database(g_db_head)) {
        return send_error(fd, keys, "ERROR: Failed to save database.");
    }
    return send_ok(fd, keys, "Role updated.");
}

/* generate RSA-2048 keypair once at startup */
static int generate_server_keypair(void) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (ctx == NULL) return 0;

    if (EVP_PKEY_keygen_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0 ||
        EVP_PKEY_keygen(ctx, &g_server_key) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }
    EVP_PKEY_CTX_free(ctx);
    return 1;
}

/*
 * TLS-style handshake:
 * 1. Send RSA public key (SPKI DER) to client
 * 2. Receive PMS encrypted with that public key (OAEP padding)
 * 3. Decrypt PMS and derive session keys
 */
static int do_handshake(int fd, SessionKeys *keys_out) {
    unsigned char *pub_der = NULL;
    int pub_len;
    uint32_t net_len;
    unsigned char enc_pms[512]; /* 2048-bit RSA output = 256 bytes max */
    uint32_t enc_net_len;
    int enc_len;
    unsigned char pms[TLS_PMS_LEN];
    size_t pms_out_len = TLS_PMS_LEN;
    EVP_PKEY_CTX *dec_ctx = NULL;
    int ret = 0;

    if (g_server_key == NULL) return 0;

    /* send public key as SPKI DER */
    pub_len = i2d_PUBKEY(g_server_key, &pub_der);
    if (pub_len <= 0 || pub_der == NULL) return 0;

    net_len = htonl((uint32_t)pub_len);
    if (!write_all(fd, (unsigned char *)&net_len, 4) ||
        !write_all(fd, pub_der, pub_len)) {
        OPENSSL_free(pub_der);
        return 0;
    }
    OPENSSL_free(pub_der);

    /* receive encrypted PMS from client */
    if (!read_all(fd, (unsigned char *)&enc_net_len, 4)) return 0;
    enc_len = (int)ntohl(enc_net_len);
    if (enc_len <= 0 || enc_len > (int)sizeof(enc_pms)) return 0;
    if (!read_all(fd, enc_pms, enc_len)) return 0;

    /* decrypt PMS with private key */
    dec_ctx = EVP_PKEY_CTX_new(g_server_key, NULL);
    if (dec_ctx == NULL) return 0;

    if (EVP_PKEY_decrypt_init(dec_ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(dec_ctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_decrypt(dec_ctx, pms, &pms_out_len, enc_pms, (size_t)enc_len) <= 0 ||
        pms_out_len != TLS_PMS_LEN) {
        EVP_PKEY_CTX_free(dec_ctx);
        return 0;
    }
    EVP_PKEY_CTX_free(dec_ctx);

    ret = derive_session_keys(pms, keys_out);
    memset(pms, 0, sizeof(pms)); /* clear PMS from stack */
    return ret;
}

/*
 * Challenge-response auth over the encrypted channel.
 * We always send a challenge and wait for the response before revealing
 * whether the username was valid, to avoid leaking info via timing.
 */
static int do_auth(int fd, const SessionKeys *keys,
                   UserRole *role_out, char username_out[MAX_USERNAME_LEN]) {
    unsigned char buf[RECORD_MAX_PLAINTEXT];
    ClientRequest *req;
    ServerChallenge challenge;
    ClientResponse *resp;
    AuthResult result;
    UserRecord *user;
    unsigned char expected[SHA256_DIGEST_LEN];
    int n;

    /* Step 1: receive ClientRequest. */
    n = recv_record(fd, keys, buf, RECORD_MAX_PLAINTEXT);
    if (n != (int)sizeof(ClientRequest)) {
        return 0;
    }
    req = (ClientRequest *)buf;
    req->username[MAX_USERNAME_LEN - 1] = '\0';

    user = find_user(g_db_head, req->username);

    /* Step 2: send challenge - always send even if user not found */
    memset(&challenge, 0, sizeof(challenge));
    if (user != NULL) {
        strncpy(challenge.salt, user->salt, SALT_HEX_LEN);
        challenge.salt[SALT_HEX_LEN] = '\0';
    }
    if (!generate_nonce(challenge.nonce, NONCE_LEN)) {
        return 0;
    }
    if (!send_record(fd, keys, (unsigned char *)&challenge, (int)sizeof(challenge))) {
        return 0;
    }

    /* Step 3: receive ClientResponse. */
    n = recv_record(fd, keys, buf, RECORD_MAX_PLAINTEXT);
    if (n != (int)sizeof(ClientResponse)) {
        return 0;
    }
    resp = (ClientResponse *)buf;

    /* Step 4: verify response = SHA-256(nonce || stored_hash) */
    memset(&result, 0, sizeof(result));
    if (user == NULL ||
        !compute_response(challenge.nonce, user->stored_hash, expected) ||
        !secure_compare(resp->response, expected, SHA256_DIGEST_LEN)) {
        result.success = 0;
        strncpy(result.message, "Authentication failed.", MAX_RESPONSE_LEN - 1);
        result.message[MAX_RESPONSE_LEN - 1] = '\0';
        send_record(fd, keys, (unsigned char *)&result, (int)sizeof(result));
        return 0;
    }

    result.success = 1;
    result.role = user->role;
    snprintf(result.message, MAX_RESPONSE_LEN, "Welcome, %s! Role: %s",
             user->username, role_to_string(user->role));

    if (!send_record(fd, keys, (unsigned char *)&result, (int)sizeof(result))) {
        return 0;
    }

    *role_out = user->role;
    strncpy(username_out, user->username, MAX_USERNAME_LEN - 1);
    username_out[MAX_USERNAME_LEN - 1] = '\0';
    return 1;
}

/*
 * Post-auth command loop. Reads commands until the client exits or
 * the connection drops. RBAC is enforced here before dispatching.
 */
static void do_command_loop(int fd, const SessionKeys *keys,
                            UserRole role, const char *username) {
    unsigned char buf[RECORD_MAX_PLAINTEXT];
    ClientCommand *cmd;
    int n;
    int is_admin_cmd;

    (void)username;

    while (1) {
        n = recv_record(fd, keys, buf, RECORD_MAX_PLAINTEXT);
        if (n != (int)sizeof(ClientCommand)) {
            break;
        }
        cmd = (ClientCommand *)buf;
        cmd->arg1[MAX_CMD_ARG_LEN - 1] = '\0';
        cmd->arg2[MAX_CMD_ARG_LEN - 1] = '\0';

        /* reject any command type outside the valid enum range */
        if (cmd->type < CMD_REQ1 || cmd->type > CMD_EXIT) {
            if (!send_error(fd, keys, "ERROR: Invalid command type.")) goto done;
            continue;
        }

        /* check if this command requires admin before doing anything */
        is_admin_cmd = (cmd->type == CMD_ADDUSER ||
                        cmd->type == CMD_LISTUSERS ||
                        cmd->type == CMD_SETROLE);

        if (is_admin_cmd && role != ROLE_ADMIN) {
            if (!send_error(fd, keys, "ERROR: Permission denied.")) break;
            continue;
        }

        switch (cmd->type) {
            case CMD_REQ1:
                if (!cmd_req1(fd, keys)) goto done;
                break;
            case CMD_REQ2:
                if (!cmd_req2(fd, keys)) goto done;
                break;
            case CMD_ADDUSER:
                if (!cmd_adduser(fd, keys, cmd)) goto done;
                break;
            case CMD_LISTUSERS:
                if (!cmd_listusers(fd, keys)) goto done;
                break;
            case CMD_SETROLE:
                if (!cmd_setrole(fd, keys, cmd)) goto done;
                break;
            case CMD_EXIT:
                send_ok(fd, keys, "Goodbye.");
                goto done;
            default:
                if (!send_error(fd, keys, "ERROR: Unknown command.")) goto done;
                break;
        }
    }

done:
    return;
}

/* handle one client connection: handshake -> auth -> commands */
static void handle_client(int client_fd) {
    SessionKeys keys;
    UserRole role;
    char username[MAX_USERNAME_LEN];

    memset(&keys, 0, sizeof(keys));

    if (!do_handshake(client_fd, &keys)) {
        fprintf(stderr, "Handshake failed\n");
        return;
    }

    if (!do_auth(client_fd, &keys, &role, username)) {
        fprintf(stderr, "Auth failed\n");
        return;
    }

    printf("Session: %s (%s)\n", username, role_to_string(role));
    do_command_loop(client_fd, &keys, role, username);
    printf("Session ended: %s\n", username);
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int opt = 1;
    int port;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    port = atoi(argv[1]);
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    if (!generate_server_keypair()) {
        fprintf(stderr, "Failed to generate RSA keypair\n");
        return 1;
    }

    g_db_head = load_database();
    if (g_db_head == NULL) {
        fprintf(stderr, "Warning: %s not found or empty. Run ./init_db first.\n", USER_DB_FILE);
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, 5) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    /* ignore SIGPIPE so the server doesn't crash if a client disconnects mid-write */
    signal(SIGPIPE, SIG_IGN);
    printf("Server listening on port %d\n", port);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }
        printf("Client connected\n");
        handle_client(client_fd);
        close(client_fd);
    }

    EVP_PKEY_free(g_server_key);
    free_database(g_db_head);
    close(server_fd);
    return 0;
}
