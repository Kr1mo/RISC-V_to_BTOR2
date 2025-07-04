# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g

# Directories
SRC_DIR = src
BIN_DIR = bin
OBJ_DIR = obj

# Files
UTILS_SRC = $(wildcard $(SRC_DIR)/utils/*.c)
UTILS_OBJ = $(patsubst $(SRC_DIR)/utils/%.c, $(OBJ_DIR)/%.o, $(UTILS_SRC))

RISCV2BTOR2_SRC = $(SRC_DIR)/riscv_to_btor2.c
RESTATE_SRC = $(SRC_DIR)/restate_witness.c
FUZZER_SRC = $(SRC_DIR)/state_fuzzer.c

RISCV2BTOR2_OBJ = $(OBJ_DIR)/riscv_to_btor2.o
RESTATE_OBJ = $(OBJ_DIR)/restate_witness.o
FUZZER_OBJ = $(OBJ_DIR)/state_fuzzer.o

# Executables
RISCV2BTOR2 = $(BIN_DIR)/riscv_to_btor2
RESTATE = $(BIN_DIR)/restate_witness
FUZZER = $(BIN_DIR)/state_fuzzer

# Targets
all: format $(RISCV2BTOR2) $(RESTATE) $(FUZZER)

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

$(FUZZER): $(FUZZER_OBJ) $(UTILS_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Built $(FUZZER)"
	@echo ""

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/utils/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean: clean_tests clean_default
	rm -rf $(OBJ_DIR) $(BIN_DIR) *.tmp *.out $(SRC_DIR)/*.state $(SRC_DIR)/*.tmp $(SRC_DIR)/*.btor2 *.state *.btor2

clean_keep_bin:
	rm -rf $(OBJ_DIR) *.tmp *.out sh_utils/*.state sh_utils/*.diff

clean_witness:
	rm -rf sh_utils/witness sh_utils/*.tmp

clean_diffs:
	rm -rf sh_utils/diffs

clean_tests: clean_witness clean_diffs clean_bench clean_default
	rm -rf sh_utils/*.log sh_utils/generated_states

clean_bench:
	rm -rf benchmark_files/temp_files bench*.log

clean_default:
	rm -rf fuzzed.state output.btor2

style-check:
	clang-tidy $(SRC_DIR)/*.c $(UTILS_SRC) -- -std=c11

format:
	clang-format -i $(SRC_DIR)/*.c $(UTILS_SRC)

ct: clean_tests

.PHONY: all clean clean_keep_bin clean_tests format style-check ct