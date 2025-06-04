#include "./state.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BTOR_MEMORY_SIZE 8 // Should be raised to 16 or 32 in future

int btor_constants(FILE *f) {
  fprintf(f, "; Basics\n");
  fprintf(f, "1 sort bitvec 1 Boolean\n"); // booleans
  fprintf(f, "2 sort bitvec %d Address_space\n",
          BTOR_MEMORY_SIZE);                   // memory representation
  fprintf(f, "3 sort bitvec 8 Memory_cell\n"); // memory cell
  fprintf(f, "4 sort bitvec 32 Command\n");    // command
  fprintf(f, "5 sort bitvec 64 Register\n");   // registers
  fprintf(f, "6 sort array 2 3 Memory\n");     // Array with BTOR_MEMORY_SIZE
                                               // elements of 8 bit memory cells
  fprintf(f, "7 zero 5\n");                    // register zero
  fprintf(f, "8 constd 4 31 register_bitmask\n"); // bitmask for register codes
  fprintf(f, "9 constd 4 7 shift_rd\n");
  fprintf(f, "10 constd 4 15 shift_rs1\n");
  fprintf(f, "11 constd 4 20 shift_rs2\n");
  fprintf(f, "12 constd 4 12 shift_funct3\n");
  fprintf(f, "13 constd 4 25 shift_funct7\n");
  fprintf(f, "14 one 4 little_helper\n");
  return 15; // Next line number
}
int btor_register_consts(FILE *f, int next_line, state *s) {
  fprintf(f, ";\n; Define Register Constants\n");
  for (size_t i = 0; i < 32; i++) {
    if (is_register_initialised(s, i) && get_register(s, i) != 0) {
      fprintf(f, "%d constd 5 %ld\n", next_line, get_register(s, i));
      next_line++;
    }
  }
  fprintf(f, "%d constd 2 %ld\n", next_line,
          s->pc % (int)pow(2, BTOR_MEMORY_SIZE));
  return next_line + 1;
}
int btor_registers(FILE *f, int next_line, int reg_const_loc, state *s) {
  fprintf(f, ";\n; Define Registers\n");
  int reg_state_loc = next_line;
  for (size_t i = 0; i < 32; i++) {
    fprintf(f, "%d state 5 x%ld\n", next_line, i);
    next_line++;
  }
  fprintf(f, "%d state 2 pc\n", next_line);
  next_line++;

  int not_null_regs = 0;
  for (size_t i = 0; i < 32; i++) {
    if (is_register_initialised(s, i) && get_register(s, i) != 0) {
      fprintf(f, "%d init 5 %ld %d\n", next_line, reg_state_loc + i,
              reg_const_loc + not_null_regs);
      not_null_regs++;
    } else {
      fprintf(f, "%d init 5 %ld 7\n", next_line, reg_state_loc + i);
    }
    next_line++;
  }
  fprintf(f, "%d init 2 %d %d\n", next_line, reg_state_loc + 32,
          reg_const_loc + not_null_regs);
  return next_line + 1;
}

int btor_memory(
    FILE *f, int next_line,
    state *s) { // Fills the memory with initialised values. Only takes the
                // first pow(2, BTOR_MEMORY_SIZE) addresses into account.
  fprintf(f, ";\n; Define memory\n");
  fprintf(f, "%d zero 3 empty_cell\n", next_line);
  int empty_cell = next_line;
  next_line++;
  fprintf(f, "%d state 6 memory_initialzer\n", next_line);
  int memory_initializer = next_line;
  next_line++;
  uint64_t *addresses = get_initialised_adresses(s->memory);

  for (size_t i = 1; i <= addresses[0]; i++) {
    if (addresses[i] >= (int)pow(2, BTOR_MEMORY_SIZE)) {
      break; // Only take the first pow(2, BTOR_MEMORY_SIZE) addresses into
             // account
    }
    if (get_byte(s, addresses[i]) ==
        0) // If the byte is 0, do not write it to memory
    {
      fprintf(f, "%d constd 2 %ld\n", next_line,
              addresses[i]); // with previous if-statement, size of addresses[i]
                             // is always smaller than 2^BTOR_MEMORY_SIZE
      fprintf(f, "%d write 6 %d %d %d\n", next_line + 1, memory_initializer,
              next_line, empty_cell);
      memory_initializer = next_line + 1;
      next_line += 2;
    } else {
      fprintf(f, "%d consth 3 %x\n", next_line, get_byte(s, addresses[i]));
      fprintf(f, "%d constd 2 %ld\n", next_line + 1,
              addresses[i] % (int)pow(2, BTOR_MEMORY_SIZE));
      fprintf(f, "%d write 6 %d %d %d\n", next_line + 2, memory_initializer,
              next_line + 1, next_line);
      memory_initializer = next_line + 2;
      next_line += 3;
    }
  }
  fprintf(f, "%d state 6 memory\n", next_line);
  fprintf(f, "%d init 6 %d %d\n", next_line + 1, next_line, memory_initializer);
  next_line += 2;
  free(addresses);
  return next_line;
}
int btor_get_current_command(FILE *f, int next_line, int registers,
                             int memory) {
  fprintf(f, ";\n; Get the current command\n");
  fprintf(f, "%d sort bitvec 16\n", next_line);
  fprintf(f, "%d one 3\n", next_line + 1);

  fprintf(f, "%d read 3 %d %d\n", next_line + 2, memory,
          registers + 32); // Read the memory at the PC address
  fprintf(f, "%d add 3 %d %d\n", next_line + 3, registers + 32, next_line + 1);
  fprintf(f, "%d read 3 %d %d\n", next_line + 4, memory,
          next_line + 3); // Read the next memory cell
  fprintf(f, "%d add 3 %d %d\n", next_line + 5, next_line + 3, next_line + 1);
  fprintf(f, "%d read 3 %d %d\n", next_line + 6, memory,
          next_line + 5); // Read the next memory cell
  fprintf(f, "%d add 3 %d %d\n", next_line + 7, next_line + 5, next_line + 1);
  fprintf(f, "%d read 3 %d %d\n", next_line + 8, memory,
          next_line + 7); // Read the last memory cell

  fprintf(f, "%d concat %d %d %d\n", next_line + 9, next_line, next_line + 4,
          next_line + 2); // Concatenate the first two memory cells
  fprintf(f, "%d concat %d %d %d\n", next_line + 10, next_line, next_line + 8,
          next_line + 6); // Concatenate the last two memory cells
  fprintf(f, "%d concat 4 %d %d\n", next_line + 11, next_line + 10,
          next_line + 9); // Concatenate all
  return next_line + 12;
}
int btor_get_opcode(FILE *f, int next_line, int command_loc) {
  fprintf(f, ";\n; Get the opcode\n");
  fprintf(f, "%d consth 4 7f\n", next_line); // bitmask
  fprintf(f, "%d and 4 %d %d\n", next_line + 1, next_line,
          command_loc); // Extract the opcode (first byte)
  return next_line + 2;
}
int btor_get_destination(FILE *f, int next_line, int command_loc) {
  fprintf(f, ";\n; Get rd\n");
  fprintf(f, "%d srl 4 %d 9\n", next_line,
          command_loc); // shift so rd is in front
  fprintf(f, "%d and 4 8 %d\n", next_line + 1, next_line); // use bitmask
  return next_line + 2;
}
int btor_get_source1(FILE *f, int next_line, int command_loc) {
  fprintf(f, ";\n; Get rs1\n");
  fprintf(f, "%d srl 4 %d 10\n", next_line,
          command_loc); // shift so rs1 is in front
  fprintf(f, "%d and 4 8 %d\n", next_line + 1, next_line); // use bitmask
  return next_line + 2;
}
int btor_get_source2(FILE *f, int next_line, int command_loc) {
  fprintf(f, ";\n; Get rs2\n");
  fprintf(f, "%d srl 4 %d 11\n", next_line,
          command_loc); // shift so rs2 is in front
  fprintf(f, "%d and 4 8 %d\n", next_line + 1, next_line); // use bitmask
  return next_line + 2;
}
int btor_get_funct3(FILE *f, int next_line, int command_loc) {
  fprintf(f, ";\n; Get funct3\n");
  fprintf(f, "%d srl 4 %d 12\n", next_line,
          command_loc); // shift so rd is in front
  fprintf(f, "%d and 4 %d 9\n", next_line + 1,
          next_line); // use shift for rd as bitmask
  return next_line + 2;
}
int btor_get_funct7(FILE *f, int next_line, int command_loc) {
  fprintf(f, ";\n; Get funct7\n");
  fprintf(f, "%d srl 4 %d 13\n", next_line,
          command_loc); // shift so funct7 is in front. No mask needed as there
                        // is nothing left of funct7
  return next_line + 1;
}
int btor_get_immediate(FILE *f, int next_line, int command_loc, int *codes) {
  fprintf(f, ";\n; Get immediate\n");
  fprintf(f, ";\n; Set valid opcodes as constants\n");
  fprintf(f, "%d constd 4 3 load\n", next_line);
  fprintf(f, "%d constd 4 19 math_i\n", next_line + 1);
  fprintf(f, "%d constd 4 23 auipc\n", next_line + 2);
  fprintf(f, "%d constd 4 27 math_wi\n", next_line + 3);
  fprintf(f, "%d constd 4 35 store\n", next_line + 4);
  fprintf(f, "%d constd 4 51 math_reg\n", next_line + 5);
  fprintf(f, "%d constd 4 55 lui\n", next_line + 6);
  fprintf(f, "%d constd 4 59 math_w\n", next_line + 7);
  fprintf(f, "%d constd 4 99 branch\n", next_line + 8);
  fprintf(f, "%d constd 4 103 jump_r\n", next_line + 9);
  fprintf(f, "%d constd 4 111 jump\n", next_line + 10);

  fprintf(f, ";\n; Get possible immediate values\n");
  fprintf(f, "%d sra 4 %d 11 i-immediate\n", next_line + 12, command_loc);

  fprintf(f, "%d sra 4 %d 11\n", next_line + 13, command_loc);
  fprintf(f, "%d and 4 %d -8\n", next_line + 14,
          next_line + 13); // mask to get i[11:5] of s-type
  fprintf(f, "%d add 4 %d %d s-immediate\n", next_line + 15, next_line + 14,
          codes[1]);

  fprintf(f, "%d and 4 %d -14 [4:0]\n", next_line + 16,
          codes[1]); // mask rd to get i[4:1] of b-type
  fprintf(f, "%d consth 4 3f\n", next_line + 17);
  fprintf(f, "%d and 4 %d %d\n", next_line + 18, next_line + 17, codes[5]);
  fprintf(f, "%d constd 4 5\n", next_line + 19);
  fprintf(f, "%d sll 4 %d %d [10:5]\n", next_line + 20, next_line + 18,
          next_line + 19);
  fprintf(f, "%d add 4 %d %d [10:0]\n", next_line + 21, next_line + 20,
          next_line + 14);
  fprintf(f, "%d sll 4 14 9\n", next_line + 22);
  fprintf(f, "%d and 4 %d %d\n", next_line + 23, next_line + 22, codes[1]);
  fprintf(f, "%d constd 4 4\n", next_line + 24);
  fprintf(f, "%d sll 4 %d %d [11]\n", next_line + 25, next_line + 23,
          next_line + 24);
  fprintf(f, "%d add 4 %d %d [11:0]\n", next_line + 26, next_line + 21,
          next_line + 25);
  fprintf(f, "%d sra 4 %d %d [31:12]\n", next_line + 27, command_loc,
          next_line + 1); // shift right by 19 -> opcode math i
  fprintf(f, "%d consth 4 fff\n", next_line + 28);
  fprintf(f, "%d and 4 %d -%d [31:12]\n", next_line + 29, next_line + 27,
          next_line + 28); // mask to get i[31:12] of u-type
  fprintf(f, "%d add 4 %d %d b-immediate\n", next_line + 30, next_line + 29,
          next_line + 27);

  fprintf(f, "%d and 4 %d -%d u-immediate\n", next_line + 31, command_loc,
          next_line + 28);

  fprintf(f, "%d and 4 %d -14 [4:0]\n", next_line + 32, codes[3]);
  fprintf(f, "%d add 4 %d %d [10:0]\n", next_line + 33, next_line + 32,
          next_line + 20);
  fprintf(f, "%d and 4 %d 14\n", next_line + 34, codes[3]);
  fprintf(f, "%d constd 4 11\n", next_line + 35);
  fprintf(f, "%d sll 4 %d %d [11]\n", next_line + 36, next_line + 34,
          next_line + 35);
  fprintf(f, "%d add 4 %d %d [11:0]\n", next_line + 37, next_line + 33,
          next_line + 36);
  fprintf(f, "%d sll 4 %d 12 [14:12]\n", next_line + 38, codes[4]);
  fprintf(f, "%d add 4 %d %d [14:0]\n", next_line + 39, next_line + 37,
          next_line + 38);
  fprintf(f, "%d sll 4 %d 11 [19:15]\n", next_line + 40, codes[2]);
  fprintf(f, "%d add 4 %d %d [19:0]\n", next_line + 41, next_line + 39,
          next_line + 40);
  fprintf(f, "%d consth 4 fffff\n", next_line + 42);
  fprintf(f, "%d sra 4 %d %d\n", next_line + 43, command_loc, next_line + 42);
  fprintf(f, "%d and 4 %d -%d [31:20]\n", next_line + 44, next_line + 43,
          next_line + 42);
  fprintf(f, "%d add 4 %d %d j-immediate\n", next_line + 45, next_line + 41,
          next_line + 44);

  fprintf(f, ";\n; sort opcodes to immediates\n");
  fprintf(f, "%d eq 1 %d %d\n", next_line + 46, next_line,
          codes[0]); // i opcode load
  fprintf(f, "%d eq 1 %d %d\n", next_line + 47, next_line + 1,
          codes[0]); // i opcode math i
  fprintf(f, "%d eq 1 %d %d\n", next_line + 48, next_line + 2,
          codes[0]); // u opcode auipc
  fprintf(f, "%d eq 1 %d %d\n", next_line + 49, next_line + 3,
          codes[0]); // i opcode math wi
  fprintf(f, "%d eq 1 %d %d\n", next_line + 50, next_line + 4,
          codes[0]); // s opcode store
  fprintf(f, "%d eq 1 %d %d\n", next_line + 51, next_line + 5,
          codes[0]); //- opcode math reg
  fprintf(f, "%d eq 1 %d %d\n", next_line + 52, next_line + 6,
          codes[0]); // u opcode lui
  fprintf(f, "%d eq 1 %d %d\n", next_line + 53, next_line + 7,
          codes[0]); //- opcode math w
  fprintf(f, "%d eq 1 %d %d\n", next_line + 54, next_line + 8,
          codes[0]); // b opcode branch
  fprintf(f, "%d eq 1 %d %d\n", next_line + 55, next_line + 9,
          codes[0]); // i opcode jump r
  fprintf(f, "%d eq 1 %d %d\n", next_line + 56, next_line + 10,
          codes[0]); // j opcode jump
  // i types: JALR(jump r), LOAD, math i, math wi
  fprintf(f, "%d or 1 %d %d\n", next_line + 57, next_line + 46, next_line + 47);
  fprintf(f, "%d or 1 %d %d\n", next_line + 58, next_line + 57, next_line + 49);
  fprintf(f, "%d or 1 %d %d\n", next_line + 59, next_line + 58, next_line + 55);
  // s types: Store -> 50
  // b types: Branch -> 54
  // u types: AUIPC, LUI
  fprintf(f, "%d or 1 %d %d\n", next_line + 60, next_line + 48, next_line + 52);
  // j types: JAL (jump) -> 56
  fprintf(f, "%d ite 4 %d %d 14 i_or_not_needed\n", next_line + 61,
          next_line + 59,
          next_line +
              12); // if not i-type, no immediate is needed. 14 is just filler
  fprintf(f, "%d ite 4 %d %d %d u\n", next_line + 62, next_line + 60,
          next_line + 31, next_line + 61);
  fprintf(f, "%d ite 4 %d %d %d s\n", next_line + 63, next_line + 50,
          next_line + 15, next_line + 62);
  fprintf(f, "%d ite 4 %d %d %d b\n", next_line + 64, next_line + 54,
          next_line + 30, next_line + 63);
  fprintf(f, "%d ite 4 %d %d %d j\n", next_line + 65, next_line + 56,
          next_line + 45, next_line + 64);
  return next_line + 66;
}

void btor_check_4_all_commands(FILE *f, int next_line, int immediate, int *codes) {
        int opcode_checks = immediate - 20; //Attention! Hacky!
}

void relational_btor(FILE *f, state *s) {
  int next_line = btor_constants(f);

  int reg_const_loc = next_line;
  next_line = btor_register_consts(f, next_line, s);
  int registers = next_line; // PC is assumed as 32th register
  next_line = btor_registers(f, next_line, reg_const_loc, s);

  next_line = btor_memory(f, next_line, s);
  int memory =
      next_line - 2; // last line is initiation, state is the line before

  next_line = btor_get_current_command(f, next_line, registers, memory);
  int command = next_line - 1;

  int codes[6]; //={opcode, rd, rs1, rs2, funct3, funct7};
  next_line = btor_get_opcode(f, next_line, command);
  codes[0] = next_line - 1; // opcode
  next_line = btor_get_destination(f, next_line, command);
  codes[1] = next_line - 1; // rd
  next_line = btor_get_source1(f, next_line, command);
  codes[2] = next_line - 1; // rs1
  next_line = btor_get_source2(f, next_line, command);
  codes[3] = next_line - 1; // rs2
  next_line = btor_get_funct3(f, next_line, command);
  codes[4] = next_line - 1; // funct3
  next_line = btor_get_funct7(f, next_line, command);
  codes[5] = next_line - 1; // funct7

  next_line = btor_get_immediate(f, next_line, command, codes);
  int immediate = next_line - 1; // immediate
}

int main(int argc, char const *argv[]) {
  state *s = create_new_state();
  if (s == NULL) {
    fprintf(stderr, "Failed to create new state.\n");
    return 1;
  }
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    kill_state(s);
    return 1;
  }
  if (!load_state((char *)argv[1], s)) {
    fprintf(stderr, "Failed to load state from file: %s\n", argv[1]);
    kill_state(s);
    return 1;
  }
  FILE *f = fopen("output.btor2", "w");

  relational_btor(f, s);

  kill_state(s);
  fclose(f);
  return 0;
}