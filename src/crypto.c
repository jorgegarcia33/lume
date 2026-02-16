#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "../include/crypto.h"

static unsigned char local_pk[crypto_kx_PUBLICKEYBYTES];
static unsigned char local_sk[crypto_kx_SECRETKEYBYTES];

static ssize_t raw_send_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;
    while (total < len) {
        ssize_t n = send(sock, p + total, len - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

static ssize_t raw_recv_all(int sock, void *buf, size_t len) {
    size_t total = 0;
    char *p = buf;
    while (total < len) {
        ssize_t n = recv(sock, p + total, len - total, MSG_WAITALL);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

int crypto_init(void) {
    if (sodium_init() < 0) {
        return -1;
    }
    crypto_kx_keypair(local_pk, local_sk);
    return 0;
}

int secure_handshake_client(SecureConn *conn) {
    unsigned char remote_pk[crypto_kx_PUBLICKEYBYTES];

    if (raw_send_all(conn->sock, local_pk, sizeof(local_pk)) <= 0)
        return -1;

    if (raw_recv_all(conn->sock, remote_pk, sizeof(remote_pk)) <= 0)
        return -1;

    if (crypto_kx_client_session_keys(conn->rx_key, conn->tx_key,
                                       local_pk, local_sk, remote_pk) != 0) {
        return -1;
    }

    return 0;
}

int secure_handshake_server(SecureConn *conn) {
    unsigned char remote_pk[crypto_kx_PUBLICKEYBYTES];

    if (raw_recv_all(conn->sock, remote_pk, sizeof(remote_pk)) <= 0)
        return -1;

    if (raw_send_all(conn->sock, local_pk, sizeof(local_pk)) <= 0)
        return -1;

    if (crypto_kx_server_session_keys(conn->rx_key, conn->tx_key,
                                       local_pk, local_sk, remote_pk) != 0) {
        return -1;
    }

    return 0;
}

ssize_t secure_send(SecureConn *conn, const void *data, size_t len) {
    size_t ciphertext_len = len + crypto_secretbox_MACBYTES;
    unsigned char *ciphertext = malloc(ciphertext_len);
    if (!ciphertext) return -1;

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    crypto_secretbox_easy(ciphertext, data, len, nonce, conn->tx_key);

    uint32_t net_len = htonl((uint32_t)ciphertext_len);

    ssize_t ret = -1;
    if (raw_send_all(conn->sock, &net_len, sizeof(net_len)) <= 0) goto cleanup;
    if (raw_send_all(conn->sock, nonce, sizeof(nonce)) <= 0) goto cleanup;
    if (raw_send_all(conn->sock, ciphertext, ciphertext_len) <= 0) goto cleanup;
    ret = (ssize_t)len;

cleanup:
    free(ciphertext);
    return ret;
}

ssize_t secure_recv(SecureConn *conn, void *buffer, size_t max_len, size_t *out_len) {
    uint32_t net_len;
    if (raw_recv_all(conn->sock, &net_len, sizeof(net_len)) <= 0)
        return -1;

    uint32_t ciphertext_len = ntohl(net_len);

    if (ciphertext_len < crypto_secretbox_MACBYTES)
        return -1;

    size_t plaintext_len = ciphertext_len - crypto_secretbox_MACBYTES;
    if (plaintext_len > max_len)
        return -1;

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    if (raw_recv_all(conn->sock, nonce, sizeof(nonce)) <= 0)
        return -1;

    unsigned char *ciphertext = malloc(ciphertext_len);
    if (!ciphertext) return -1;

    if (raw_recv_all(conn->sock, ciphertext, ciphertext_len) <= 0) {
        free(ciphertext);
        return -1;
    }

    if (crypto_secretbox_open_easy(buffer, ciphertext, ciphertext_len,
                                    nonce, conn->rx_key) != 0) {
        free(ciphertext);
        return -1;
    }

    free(ciphertext);
    if (out_len) *out_len = plaintext_len;
    return (ssize_t)plaintext_len;
}
