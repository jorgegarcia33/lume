#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <pthread.h>
#include "network.h"

typedef struct {
    char local_username[USERNAME_LEN];
    int local_tcp_port;

    Peer peers[MAX_PEERS];
    int peer_count;
    pthread_mutex_t peer_mutex;

    WINDOW *win_header;
    WINDOW *win_chat;
    WINDOW *win_input;
    pthread_mutex_t chat_mutex;

    int selected_peer_index;
    int running;
} AppState;

extern AppState app_state;

void init_ui();
void cleanup_ui();
void draw_interface();
void log_message(const char *fmt, ...);
void handle_input();
void show_help();

#endif
