#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include "../include/network.h"
#include "../include/ui.h"

static ssize_t send_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;
    while (total < len) {
        ssize_t n = send(sock, p + total, len - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

int get_local_ip(char *ip_buffer, size_t buffer_size) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;

    if (getifaddrs(&ifaddr) == -1) {
        return 0;
    }

    // Look for first non-loopback IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];

            if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip)) == NULL) {
                continue;
            }
            // Skip loopback addresses
            if (strcmp(ip, "127.0.0.1") != 0) {
                strncpy(ip_buffer, ip, buffer_size - 1);
                ip_buffer[buffer_size - 1] = '\0';
                found = 1;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return found;
}

void *beacon_sender(void *arg) {
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return NULL;

    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        close(sock);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);
    addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    BeaconPacket packet;
    while (app_state.running) {
        strncpy(packet.username, app_state.local_username, USERNAME_LEN);
        packet.tcp_port = app_state.local_tcp_port;

        sendto(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
        sleep(3);
    }

    close(sock);
    return NULL;
}

void *beacon_receiver(void *arg) {
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return NULL;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BROADCAST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("UDP bind failed");
        close(sock);
        return NULL;
    }

    BeaconPacket packet;
    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);

    while (app_state.running) {
        ssize_t len = recvfrom(sock, &packet, sizeof(packet), 0, (struct sockaddr *)&sender_addr, &sender_len);
        if (len == sizeof(packet)) {
            if (strcmp(packet.username, app_state.local_username) == 0) continue;

            pthread_mutex_lock(&app_state.peer_mutex);
            int found = 0;
            for (int i = 0; i < app_state.peer_count; i++) {
                if (strcmp(app_state.peers[i].username, packet.username) == 0) {
                    app_state.peers[i].last_seen = time(NULL);
                    app_state.peers[i].ip_addr = sender_addr.sin_addr;
                    app_state.peers[i].tcp_port = packet.tcp_port;
                    found = 1;
                    break;
                }
            }
            if (!found && app_state.peer_count < MAX_PEERS) {
                Peer new_peer;
                strncpy(new_peer.username, packet.username, USERNAME_LEN);
                new_peer.ip_addr = sender_addr.sin_addr;
                new_peer.tcp_port = packet.tcp_port;
                new_peer.last_seen = time(NULL);
                app_state.peers[app_state.peer_count++] = new_peer;
                log_message("New peer discovered: %s", new_peer.username);
            }
            pthread_mutex_unlock(&app_state.peer_mutex);
        }
    }

    close(sock);
    return NULL;
}

void *connection_handler(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    MessagePacket header;
    while (recv(sock, &header, sizeof(header), MSG_WAITALL) > 0) {
        if (header.type == MSG_TEXT) {
            char *buffer = malloc(header.payload_len + 1);
            if (buffer) {
                if ((size_t)recv(sock, buffer, header.payload_len, MSG_WAITALL) == header.payload_len) {
                    buffer[header.payload_len] = '\0';
                    log_message("%s: %s", header.sender_name, buffer);
                }
                free(buffer);
            }
        } else if (header.type == MSG_FILE_METADATA) {
            FileMetadata meta;
            if (recv(sock, &meta, sizeof(meta), MSG_WAITALL) != sizeof(meta)) break;

            char *filename = strrchr(meta.filename, '/');
            if (filename) filename++;
            else filename = meta.filename;

            log_message("%s is sending file: %s (%ld bytes)", header.sender_name, filename, meta.file_size);

            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd >= 0) {
                size_t total_received = 0;
                char buffer[CHUNK_SIZE];
                while (total_received < meta.file_size) {
                    MessagePacket chunk_header;
                    if (recv(sock, &chunk_header, sizeof(chunk_header), MSG_WAITALL) != sizeof(chunk_header)) break;
                    if (chunk_header.type != MSG_FILE_CHUNK) break;

                    if ((size_t)recv(sock, buffer, chunk_header.payload_len, MSG_WAITALL) != chunk_header.payload_len) break;

                    write(fd, buffer, chunk_header.payload_len);
                    total_received += chunk_header.payload_len;
                }
                close(fd);
                log_message("File received: %s", filename);
            }
        }
    }

    close(sock);
    return NULL;
}

void *tcp_server(void *arg) {
    (void)arg;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(app_state.local_tcp_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_message("Error: Could not bind to TCP port %d. Is it already in use?", app_state.local_tcp_port);
        close(sock);
        return NULL;
    }

    if (listen(sock, 5) < 0) {
        close(sock);
        return NULL;
    }

    while (app_state.running) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int client_sock = accept(sock, (struct sockaddr *)&client_addr, &len);
        if (client_sock >= 0) {
            int *client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            pthread_t tid;
            pthread_create(&tid, NULL, connection_handler, client_sock_ptr);
            pthread_detach(tid);
        }
    }

    close(sock);
    return NULL;
}

void init_network_threads() {
    pthread_t tid;
    pthread_create(&tid, NULL, beacon_sender, NULL);
    pthread_detach(tid);
    pthread_create(&tid, NULL, beacon_receiver, NULL);
    pthread_detach(tid);
    pthread_create(&tid, NULL, tcp_server, NULL);
    pthread_detach(tid);
}

void send_text_message(int peer_index, const char *msg) {
    if (peer_index < 0 || peer_index >= app_state.peer_count) return;

    Peer peer = app_state.peers[peer_index];
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer.tcp_port);
    addr.sin_addr = peer.ip_addr;

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        MessagePacket header;
        header.type = MSG_TEXT;
        strncpy(header.sender_name, app_state.local_username, USERNAME_LEN);
        header.payload_len = strlen(msg);
        send_all(sock, &header, sizeof(header));
        send_all(sock, msg, header.payload_len);
        log_message("Me -> %s: %s", peer.username, msg);
    } else {
        log_message("Failed to connect to %s", peer.username);
    }
    close(sock);
}

void send_file(int peer_index, const char *filepath) {
    if (peer_index < 0 || peer_index >= app_state.peer_count) return;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        log_message("Failed to open file: %s", filepath);
        return;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    Peer peer = app_state.peers[peer_index];
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        close(fd);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(peer.tcp_port);
    addr.sin_addr = peer.ip_addr;

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        MessagePacket header;
        header.type = MSG_FILE_METADATA;
        strncpy(header.sender_name, app_state.local_username, USERNAME_LEN);
        header.payload_len = sizeof(FileMetadata);
        send_all(sock, &header, sizeof(header));

        FileMetadata meta;
        strncpy(meta.filename, filepath, 255);
        meta.file_size = fsize;
        send_all(sock, &meta, sizeof(meta));

        char buffer[CHUNK_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            header.type = MSG_FILE_CHUNK;
            strncpy(header.sender_name, app_state.local_username, USERNAME_LEN);
            header.payload_len = bytes_read;
            if (send_all(sock, &header, sizeof(header)) <= 0) break;
            if (send_all(sock, buffer, bytes_read) <= 0) break;
        }
        log_message("Sent file %s to %s", filepath, peer.username);
    } else {
        log_message("Failed to connect to %s", peer.username);
    }
    close(sock);
    close(fd);
}
