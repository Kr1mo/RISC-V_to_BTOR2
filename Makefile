# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Directories
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

# Files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
RISCV2BTOR2 = $(BIN_DIR)/riscv_to_btor2
RESTATE = $(BIN_DIR)/restate_witness

EXECUTABLE = $(RISCV2BTOR2) $(RESTATE)

# Targets
all: format $(RISCV2BTOR2) $(RESTATE)

$(EXECUTABLE): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

style-check:
	clang-tidy $(SOURCES) -- -std=c11

format:
	clang-format -i $(SOURCES)

.PHONY: all clean format style-check