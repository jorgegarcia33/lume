CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -pthread -std=c11 -D_DEFAULT_SOURCE
LDFLAGS = -lncurses -lpthread -lsodium
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TARGET = lume

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

PREFIX = /usr/local

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

install: all
	install -m 755 $(BIN_DIR)/$(TARGET) $(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)

.PHONY: all clean install uninstall
