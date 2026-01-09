#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/network.h"
#include "../include/ui.h"

/*
 * Load configuration from ~/.config/lume/lume.conf
 *
 * Expected format:
 *   username=user
 *   port=8080
 *
 * Returns 1 on success (only if BOTH username and port are present),
 * 0 on failure (including when only one of them is configured).
 */
int load_config_file(char *username, int *port) {
    char path[512];
    const char *home = getenv("HOME");
    if (!home) return 0;

    snprintf(path, sizeof(path), "%s/.config/lume/lume.conf", home);
    FILE *file = fopen(path, "r");
    if (!file) {
        return 0; // config file not found
    }

    char line[256];
    int found_username = 0;
    int found_port = 0;

    while (fgets(line, sizeof(line), file)) {
        size_t len = strlen(line);

        /* If the line does not end with a newline and we're not at EOF,
         * it was longer than the buffer; discard the rest of this physical line
         * so that the remainder is not treated as a separate config line.
         */
        if (len > 0 && line[len - 1] != '\n' && !feof(file)) {
            int c;
            while ((c = fgetc(file)) != '\n' && c != EOF) {
                /* discard */
            }
            /* Skip parsing this overlong line */
            continue;
        }

        // Remove trailing newline, if present
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (strncmp(line, "username=", 9) == 0) {
            char *val = line + 9;
            while (isspace((unsigned char)*val)) val++;
            char *end = val + strlen(val) - 1;
            while (end > val && isspace((unsigned char)*end)) *end-- = '\0';

            memset(username, 0, USERNAME_LEN);
            strncpy(username, val, USERNAME_LEN - 1);
            if (username[0] != '\0') {
                found_username = 1;
            }
        } else if (strncmp(line, "port=", 5) == 0) {
            char *val = line + 5;
            while (isspace((unsigned char)*val)) val++;
            char *endptr;
            long port_val = strtol(val, &endptr, 10);
            while (isspace((unsigned char)*endptr)) endptr++;
            if (endptr != val && (*endptr == '\0' || *endptr == '\r') && port_val >= 1 && port_val <= 65535) {
                *port = (int)port_val;
                found_port = 1;
            }
        }
    }

    fclose(file);
    return found_username && found_port;
}

void save_config_file(const char *username, int port) {
    char path[512];
    char dir[512];
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        return;
    }

    snprintf(dir, sizeof(dir), "%s/.config/lume", home);
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0700) == -1) {
            perror("mkdir");
            return;
        }
    }

    snprintf(path, sizeof(path), "%s/.config/lume/lume.conf", home);
    FILE *file = fopen(path, "w");
    if (!file) {
        perror("fopen");
        return;
    }

    fprintf(file, "username=%s\n", username);
    fprintf(file, "port=%d\n", port);
    fclose(file);
    printf("Configuration saved to %s\n", path);
}

void handle_config_command(int argc, char * const * argv) {
    char username[USERNAME_LEN];
    int port;

    if (argc == 2) {
        printf("Enter username: ");
        if (fgets(username, sizeof(username), stdin) == NULL) return;
        username[strcspn(username, "\n")] = 0;

        printf("Enter port: ");
        char port_str[10];
        if (fgets(port_str, sizeof(port_str), stdin) == NULL) return;
        port = atoi(port_str);
        if (port < 1 || port > 65535) {
            fprintf(stderr, "Invalid port. Port must be between 1 and 65535.\n");
            return;
        }
    } else {
        fprintf(stderr, "Usage: %s config\n", argv[0]);
        return;
    }

    save_config_file(username, port);
}

int main(int argc, char *argv[]) {
    char config_username[USERNAME_LEN];
    int config_port = 0;

    if (argc >= 2 && strcmp(argv[1], "config") == 0) {
        handle_config_command(argc, argv);
        return 0;
    }

    if (argc == 3) {
        // 1️⃣ Use command-line arguments
        memset(app_state.local_username, 0, USERNAME_LEN);
        strncpy(app_state.local_username, argv[1], USERNAME_LEN - 1);

        char *endptr;
        long val = strtol(argv[2], &endptr, 10);
        if (*argv[2] != '\0' && *endptr == '\0' && val >= 1 && val <= 65535) {
            app_state.local_tcp_port = (int)val;
        } else {
            fprintf(stderr, "Invalid port: %s. Port must be between 1 and 65535.\n", argv[2]);
            return 1;
        }

    } else if (argc == 1 && load_config_file(config_username, &config_port)) {
        // 2️⃣ Fallback to config file when no CLI arguments are provided
        memset(app_state.local_username, 0, USERNAME_LEN);
        strncpy(app_state.local_username, config_username, USERNAME_LEN - 1);
        app_state.local_tcp_port = config_port;

    } else {
        // 3️⃣ Invalid args or no config available
        if (argc == 1) {
            fprintf(stderr, "Error: Configuration file not found at ~/.config/lume/lume.conf\n");
            fprintf(stderr, "Please run '%s config' to set up your profile or provide arguments:\n", argv[0]);
        } else {
            fprintf(stderr, "Usage:\n");
        }
        fprintf(stderr, "  %s <username> <tcp_port>  (Start with specific settings)\n", argv[0]);
        fprintf(stderr, "  %s config                 (Configure interactively)\n", argv[0]);
        fprintf(stderr, "  %s                        (Start using saved config)\n", argv[0]);
        return 1;
    }

    // Get local IP address
    if (!get_local_ip(app_state.local_ip, sizeof(app_state.local_ip))) {
        strncpy(app_state.local_ip, "unknown", sizeof(app_state.local_ip));
    }

    init_ui();
    init_network_threads();

    log_message("Welcome to Lume, %s!", app_state.local_username);
    log_message("Listening on port %d...", app_state.local_tcp_port);
    log_message("Type /help for available commands");

    handle_input();

    cleanup_ui();
    return 0;
}
