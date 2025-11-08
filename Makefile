# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

# Linker libraries (Readline)
LDLIBS = -lreadline

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Target binary
TARGET = $(BIN_DIR)/myshell

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Default rule
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Compiling
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Run the shell
run: all
	./$(TARGET)

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all run clean
