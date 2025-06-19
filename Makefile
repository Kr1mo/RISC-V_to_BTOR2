# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

# Directories
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

# Files
UTILS_SRC = $(wildcard $(SRC_DIR)/utils/*.c)
UTILS_OBJ = $(patsubst $(SRC_DIR)/utils/%.c, $(OBJ_DIR)/%.o, $(UTILS_SRC))

RISCV2BTOR2_SRC = $(SRC_DIR)/riscv_to_btor2.c
RESTATE_SRC = $(SRC_DIR)/restate_witness.c

RISCV2BTOR2_OBJ = $(OBJ_DIR)/riscv_to_btor2.o
RESTATE_OBJ = $(OBJ_DIR)/restate_witness.o

# Executables
RISCV2BTOR2 = $(BIN_DIR)/riscv_to_btor2
RESTATE = $(BIN_DIR)/restate_witness

# Targets
all: format $(RISCV2BTOR2) $(RESTATE)

$(RISCV2BTOR2): $(RISCV2BTOR2_OBJ) $(UTILS_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Built $(RISCV2BTOR2)"
	@echo ""

$(RESTATE): $(RESTATE_OBJ) $(UTILS_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Built $(RESTATE)"
	@echo ""

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/utils/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

style-check:
	clang-tidy $(SRC_DIR)/*.c $(UTILS_SRC) -- -std=c11

format:
	clang-format -i $(SRC_DIR)/*.c $(UTILS_SRC)

.PHONY: all clean format style-check