#ifndef CRYPTO_H
#define CRYPTO_H

#include <sodium.h>
#include <sys/types.h>

typedef struct SecureConn {
    int sock;
    unsigned char rx_key[crypto_kx_SESSIONKEYBYTES];
    unsigned char tx_key[crypto_kx_SESSIONKEYBYTES];
} SecureConn;

int crypto_init(void);
int secure_handshake_client(SecureConn *conn);
int secure_handshake_server(SecureConn *conn);
ssize_t secure_send(SecureConn *conn, const void *data, size_t len);
ssize_t secure_recv(SecureConn *conn, void *buffer, size_t max_len, size_t *out_len);

#endif
