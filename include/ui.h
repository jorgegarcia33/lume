#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <pthread.h>
#include "network.h"

typedef struct {
    char local_username[USERNAME_LEN];
    int local_tcp_port;
    char local_ip[16];

    Peer peers[MAX_PEERS];
    int peer_count;
    pthread_mutex_t peer_mutex;

    WINDOW *win_header;
    WINDOW *win_chat;
    WINDOW *win_input;
    pthread_mutex_t chat_mutex;

    int selected_peer_index;
    int running;

    // File transfer confirmation
    int pending_file_transfer;
    char pending_sender[USERNAME_LEN];
    char pending_filename[256];
    size_t pending_filesize;
    int pending_sock;
    time_t pending_transfer_time;
    pthread_mutex_t file_transfer_mutex;

} AppState;

extern AppState app_state;

void init_ui();
void cleanup_ui();
void draw_interface();
void log_message(const char *fmt, ...);
void handle_input();
void show_help();
void show_file_prompt(const char *sender, const char *filename, size_t filesize);
void accept_file_transfer();
void reject_file_transfer();

#endif
