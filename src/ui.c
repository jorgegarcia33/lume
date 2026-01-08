#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ncurses.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../include/ui.h"

AppState app_state;

void init_ui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    curs_set(1);


    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_GREEN, -1);
    init_pair(4, COLOR_CYAN, -1);
    init_pair(5, COLOR_WHITE, -1);

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    app_state.win_header = newwin(3, max_x, 0, 0);
    app_state.win_chat = newwin(max_y - 6, max_x, 3, 0);
    app_state.win_input = newwin(3, max_x, max_y - 3, 0);

    wbkgd(app_state.win_header, COLOR_PAIR(1));
    wbkgd(app_state.win_chat, COLOR_PAIR(1));
    wbkgd(app_state.win_input, COLOR_PAIR(1));

    scrollok(app_state.win_chat, TRUE);

    pthread_mutex_init(&app_state.peer_mutex, NULL);
    pthread_mutex_init(&app_state.chat_mutex, NULL);

    app_state.peer_count = 0;
    app_state.selected_peer_index = -1;
    app_state.running = 1;

    refresh();
}

void cleanup_ui() {
    delwin(app_state.win_header);
    delwin(app_state.win_chat);
    delwin(app_state.win_input);
    endwin();
    pthread_mutex_destroy(&app_state.peer_mutex);
    pthread_mutex_destroy(&app_state.chat_mutex);
}

void draw_interface() {
    pthread_mutex_lock(&app_state.peer_mutex);

    wclear(app_state.win_header);
    box(app_state.win_header, 0, 0);

    int max_y, max_x;
    getmaxyx(app_state.win_header, max_y, max_x);
    (void)max_y;  // Unused, but needed for getmaxyx

    // Display local user info with IP address
    mvwprintw(app_state.win_header, 1, 2, "Lume - %s [%s:%d]",
              app_state.local_username,
              app_state.local_ip,
              app_state.local_tcp_port);

    if (app_state.peer_count > 0) {
        if (app_state.selected_peer_index == -1) app_state.selected_peer_index = 0;
        if (app_state.selected_peer_index >= app_state.peer_count) app_state.selected_peer_index = 0;

        Peer selected = app_state.peers[app_state.selected_peer_index];
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &selected.ip_addr, ip_str, sizeof(ip_str)) == NULL) {
            strncpy(ip_str, "?", sizeof(ip_str));
            ip_str[sizeof(ip_str) - 1] = '\0';
        }

        wattron(app_state.win_header, COLOR_PAIR(3));

        // Calculate length of local user info to prevent overlap
        int local_info_len = snprintf(NULL, 0, "Lume - %s [%s:%d]",
                                      app_state.local_username,
                                      app_state.local_ip,
                                      app_state.local_tcp_port);

        // Position peer info with safe spacing (at least 5 chars gap)
        int peer_col = local_info_len + 7;

        // Fallback to midpoint if calculated position is too far right
        if (peer_col > max_x / 2) {
            peer_col = max_x / 2;
        }

        mvwprintw(app_state.win_header, 1, peer_col, "To: %s [%s:%d] (%d/%d)",
            selected.username,
            ip_str,
            selected.tcp_port,
            app_state.selected_peer_index + 1,
            app_state.peer_count);
        wattroff(app_state.win_header, COLOR_PAIR(3));
    } else {
        wattron(app_state.win_header, COLOR_PAIR(2));
        // Right-align "Scanning for peers..." to prevent overlap
        const char *scan_msg = "Scanning for peers...";
        int scan_col = max_x - strlen(scan_msg) - 3;
        if (scan_col < 2) {
            scan_col = 2;
        }
        mvwprintw(app_state.win_header, 1, scan_col, "%s", scan_msg);
        wattroff(app_state.win_header, COLOR_PAIR(2));
    }
    wrefresh(app_state.win_header);

    pthread_mutex_unlock(&app_state.peer_mutex);

    box(app_state.win_input, 0, 0);
    mvwprintw(app_state.win_input, 1, 2, "> ");
    wrefresh(app_state.win_input);

    pthread_mutex_lock(&app_state.chat_mutex);
    wrefresh(app_state.win_chat);
    pthread_mutex_unlock(&app_state.chat_mutex);
}

void log_message(const char *fmt, ...) {
    pthread_mutex_lock(&app_state.chat_mutex);

    time_t rawtime;
    const struct tm *timeinfo;
    char timestamp[12];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", timeinfo);

    wattron(app_state.win_chat, COLOR_PAIR(5) | A_DIM);
    wprintw(app_state.win_chat, "%s", timestamp);
    wattroff(app_state.win_chat, COLOR_PAIR(5) | A_DIM);

    int color = 2;
    if (strstr(fmt, "Me ->") == fmt) {
        color = 3;
    } else if (strstr(fmt, ": ") != NULL && strstr(fmt, "Me ->") == NULL) {
        color = 4;
    }

    wattron(app_state.win_chat, COLOR_PAIR(color));

    va_list args;
    va_start(args, fmt);
    vw_printw(app_state.win_chat, fmt, args);
    wprintw(app_state.win_chat, "\n");
    va_end(args);

    wattroff(app_state.win_chat, COLOR_PAIR(color));

    wrefresh(app_state.win_chat);
    pthread_mutex_unlock(&app_state.chat_mutex);
}

void handle_input() {
    char input_buf[256];
    int input_pos = 0;
    memset(input_buf, 0, sizeof(input_buf));

    nodelay(app_state.win_input, TRUE);
    keypad(app_state.win_input, TRUE);

    while (app_state.running) {
        draw_interface();

        mvwprintw(app_state.win_input, 1, 4, "%s", input_buf);
        wrefresh(app_state.win_input);

        int ch = wgetch(app_state.win_input);
        if (ch != ERR) {
            if (ch == KEY_UP) {
                pthread_mutex_lock(&app_state.peer_mutex);
                if (app_state.peer_count > 0) {
                    app_state.selected_peer_index = (app_state.selected_peer_index + 1) % app_state.peer_count;
                }
                pthread_mutex_unlock(&app_state.peer_mutex);
            } else if (ch == KEY_DOWN) {
                pthread_mutex_lock(&app_state.peer_mutex);
                if (app_state.peer_count > 0) {
                    app_state.selected_peer_index = (app_state.selected_peer_index - 1 + app_state.peer_count) % app_state.peer_count;
                }
                pthread_mutex_unlock(&app_state.peer_mutex);
            } else if (ch == '\n') {
                if (input_pos > 0) {
                    if (strncmp(input_buf, "/file ", 6) == 0) {
                        send_file(app_state.selected_peer_index, input_buf + 6);
                    } else {
                        send_text_message(app_state.selected_peer_index, input_buf);
                    }
                    memset(input_buf, 0, sizeof(input_buf));
                    input_pos = 0;
                    wclear(app_state.win_input);
                }
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (input_pos > 0) {
                    input_buf[--input_pos] = '\0';
                    mvwprintw(app_state.win_input, 1, 4 + input_pos, " ");
                }
            } else if (input_pos < 255 && ch >= 32 && ch <= 126) {
                input_buf[input_pos++] = ch;
                input_buf[input_pos] = '\0';
            } else if (ch == 27) {
                app_state.running = 0;
            }
        }
        usleep(10000);
    }
}
