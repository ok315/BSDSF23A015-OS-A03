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
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Default rule
all: $(TARGET)

# Ensure bin dir exists (order-only prerequisite so it doesn't force rebuilds)
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Compile rule; ensure obj dir exists
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create bin/ and obj/ directories (order-only)
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Run the shell
run: all
	./$(TARGET)

# Clean build files
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all run clean

