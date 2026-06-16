DEBUG = 1

CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Wconversion -fwrapv -std=c11 -pthread

ifeq ($(DEBUG), 1)
CFLAGS += -g
else
CFLAGS += -O3 -DNDEBUG
endif

SRC_DIR = srcs
OBJ_DIR = objs
BIN_DIR = bin

ALLOCATOR_DIR = Allocator
DATA_STRUCTURE_DIR = Data_structure
LANG_DIR = Lang
UTILS_DIR = Utils

ALLOCATOR_SRCS = $(wildcard $(SRC_DIR)/$(ALLOCATOR_DIR)/*.c)
ALLOCATOR_OBJS = $(patsubst $(SRC_DIR)/$(ALLOCATOR_DIR)/%.c, $(OBJ_DIR)/$(ALLOCATOR_DIR)_%.o, $(ALLOCATOR_SRCS))

DATA_STRUCTURE_SRCS = $(wildcard $(SRC_DIR)/$(DATA_STRUCTURE_DIR)/*.c)
DATA_STRUCTURE_OBJS = $(patsubst $(SRC_DIR)/$(DATA_STRUCTURE_DIR)/%.c, $(OBJ_DIR)/$(DATA_STRUCTURE_DIR)_%.o, $(DATA_STRUCTURE_SRCS))

LANG_SRCS = $(wildcard $(SRC_DIR)/$(LANG_DIR)/*.c)
LANG_OBJS = $(patsubst $(SRC_DIR)/$(LANG_DIR)/%.c, $(OBJ_DIR)/$(LANG_DIR)_%.o, $(LANG_SRCS))

UTILS_SRCS = $(wildcard $(SRC_DIR)/$(UTILS_DIR)/*.c)
UTILS_OBJS = $(patsubst $(SRC_DIR)/$(UTILS_DIR)/%.c, $(OBJ_DIR)/$(UTILS_DIR)_%.o, $(UTILS_SRCS))

SRC = main.c
OBJ = $(SRC:.c=.o)
ifeq ($(OS), Windows_NT)
BIN = $(SRC:.c=.exe)
else
BIN = $(SRC:.c=)
endif

.PHONY: all remake clean

all: $(BIN_DIR)/$(BIN)

remake: clean all

$(BIN_DIR)/$(BIN): $(OBJ_DIR)/$(OBJ) $(ALLOCATOR_OBJS) $(DATA_STRUCTURE_OBJS) $(LANG_OBJS) $(UTILS_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/$(OBJ): $(SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/$(ALLOCATOR_DIR)_%.o: $(SRC_DIR)/$(ALLOCATOR_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/$(DATA_STRUCTURE_DIR)_%.o: $(SRC_DIR)/$(DATA_STRUCTURE_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/$(LANG_DIR)_%.o: $(SRC_DIR)/$(LANG_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/$(UTILS_DIR)_%.o: $(SRC_DIR)/$(UTILS_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir $@

$(OBJ_DIR):
	mkdir $@

ifeq ($(OS), Windows_NT)
clean:
	del /f /q $(BIN_DIR)\$(BIN) $(OBJ_DIR)\*
else
clean:
	rm -rf $(BIN_DIR)/$(BIN) $(OBJ_DIR)/*
endif
