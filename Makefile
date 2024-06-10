# Define the compiler
CC := gcc

# Define any compile-time flags
CFLAGS := -Wall -g

# Define the directory containing source files
SRC_DIR := src

# Define the directory for compiled object files
OBJ_DIR := bin/obj

# Define the directory for the final binary
BIN_DIR := bin

# Specify the final binary name
TARGET := $(BIN_DIR)/smtp_auto

# Automatically list all C source files in the SRC_DIR
SOURCES := $(wildcard $(SRC_DIR)/*.c)

# Define corresponding object files in OBJ_DIR (same base names as source files)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Default target
all: $(TARGET)

# Link all object files to create the final binary
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@

# Compile each C source file to an object file
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up compiled files
clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

# This is a 'phony' target, which means it's not a real file name
.PHONY: all clean
