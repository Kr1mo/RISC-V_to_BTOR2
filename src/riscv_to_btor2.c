#include "./utils/state.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BTOR_MEMORY_SIZE 8 // Should be raised to 16 or 32 in future

int btor_constants(FILE *f) { // This sadly grew as-needed
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
  fprintf(f, "14 one 4 bit_picker\n");
  fprintf(f, "15 one 1 true\n");
  fprintf(f, "16 zero 1 false\n");
  return 17; // Next line number
}
int btor_counter(FILE *f, int next_line) {
  fprintf(f, ";\n; Counter for executed commands\n");
  fprintf(f, "%d zero 4\n", next_line);    // Initial value of the counter
  fprintf(f, "%d one 4\n", next_line + 1); // Constant one
  fprintf(f, "%d state 4 iterations_counter\n",
          next_line + 2); // with 32bit this falls out of line. This is wanted
                          // behaviour because this is not part of risc-v
  fprintf(f, "%d init 4 %d %d\n", next_line + 3, next_line + 2, next_line);
  fprintf(f, "%d add 4 %d %d\n", next_line + 4, next_line + 1, next_line + 2);
  fprintf(f, "%d next 4 %d %d /n", next_line + 5, next_line + 2, next_line + 4);
  return next_line + 6; // Next line number
}

int btor_register_initialisation_flags(FILE *f, int next_line, state *s) {
  fprintf(f, ";\n; Define Register Initialisation flags\n");
  int reg_flags_loc = next_line;
  for (uint8_t i = 0; i < 32; i++) {
    fprintf(f, "%d state 1 flag_x%d\n", next_line, i);
    next_line++;
  }
  for (uint8_t i = 0; i < 32; i++) {
    if (is_register_initialised(s, i)) {
      fprintf(f, "%d init 1 %d 15\n", next_line,
              reg_flags_loc + i); // 15 is true
    } else {
      fprintf(f, "%d init 1 %d 16\n", next_line,
              reg_flags_loc + i); // 16 is false
    }
    next_line++;
  }
  return next_line;
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
  fprintf(f, "%d init 6 %d %d\n", next_line + 1, next_line,
          empty_cell); // Initialise the memory with empty cells
  int memory_initializer = next_line;
  next_line += 2;
  uint64_t *addresses = get_initialised_adresses(s->memory);

  for (size_t i = 1; i <= addresses[0]; i++) { // TODO also initialise 0s??
    if (addresses[i] >= (int)pow(2, BTOR_MEMORY_SIZE)) {
      break; // Only take the first pow(2, BTOR_MEMORY_SIZE) addresses into
             // account
    }
    if (get_byte(s, addresses[i]) == 0) {
      fprintf(f, "%d constd 2 %ld\n", next_line,
              addresses[i]); // with previous if-statement, size of addresses[i]
      // is always smaller than 2^BTOR_MEMORY_SIZE
      int mem_adr = next_line;
      next_line++;
      // If first byte is 0, this does not change zero initialised memory and
      // btormc does not track. therefore, I created a change that will be
      // overwritten, so 0-bytes will always be tracked.
      if (i == 1) {
        fprintf(f, "%d one 3", next_line);
        fprintf(f, "%d write 6 %d %d %d", next_line + 1, memory_initializer,
                mem_adr, next_line);
        memory_initializer = next_line + 1;
        next_line += 2;
      }
      fprintf(f, "%d write 6 %d %d %d\n", next_line, memory_initializer,
              mem_adr, empty_cell);
      memory_initializer = next_line;
      next_line++;
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
  fprintf(f, "%d consth 4 07f\n", next_line); // bitmask
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

  int opcode_const_loc = next_line;
  next_line += 11;

  fprintf(f, ";\n; Get possible immediate values\n");
  fprintf(f, "%d sra 4 %d 11 i-immediate\n", next_line,
          command_loc); // shift right by 20
  int i_immediate_loc = next_line;
  next_line++;
  ;

  fprintf(f, "%d sra 4 %d 11\n", next_line, command_loc);
  fprintf(f, "%d and 4 %d -8\n", next_line + 1,
          next_line); // mask to remove lower 5 bit to get i[11:5] of s-type
  fprintf(f, "%d add 4 %d %d s-immediate\n", next_line + 2, next_line + 1,
          codes[1]); // add rd code as rd is i[4:0]
  int s_immediate_loc = next_line + 2;
  next_line += 3;

  fprintf(f, "%d and 4 %d -14 [4:0]\n", next_line,
          codes[1]); // mask rd to get i[4:1] of b-type
  fprintf(f, "%d consth 4 3f\n", next_line + 1);
  fprintf(f, "%d and 4 %d %d\n", next_line + 2, next_line + 1,
          codes[5]); // mask funct7 to lower 6bit get i[10:5] of b-type
  fprintf(f, "%d constd 4 5\n", next_line + 3);
  fprintf(f, "%d sll 4 %d %d [10:5]\n", next_line + 4, next_line + 2,
          next_line + 3); // move to correct place
  fprintf(f, "%d add 4 %d %d [10:0]\n", next_line + 5, next_line + 4,
          next_line); // combine
  fprintf(f, "%d sll 4 14 9\n", next_line + 6);
  fprintf(f, "%d and 4 %d %d\n", next_line + 7, next_line + 6, command_loc);
  fprintf(f, "%d constd 4 4\n", next_line + 8);
  fprintf(f, "%d sll 4 %d %d [11]\n", next_line + 9, next_line + 7,
          next_line + 8);
  fprintf(f, "%d add 4 %d %d [11:0]\n", next_line + 10, next_line + 5,
          next_line + 9); // combine
  fprintf(f, "%d sra 4 %d %d\n", next_line + 11, command_loc,
          opcode_const_loc + 1); // shift right by 19 -> opcode math i
  fprintf(f, "%d consth 4 fff\n", next_line + 12);
  fprintf(f, "%d and 4 %d -%d [31:12]\n", next_line + 13, next_line + 11,
          next_line + 12); // mask to get i[31:12] of u-type
  fprintf(f, "%d add 4 %d %d b-immediate\n", next_line + 14, next_line + 13,
          next_line + 10);
  int b_immediate_loc = next_line + 14;
  next_line += 15;

  fprintf(f, "%d and 4 %d -%d u-immediate\n", next_line, command_loc,
          b_immediate_loc - 2); // HACKY! reuse mask from b-type to get u[31:12]
  int u_immediate_loc = next_line;
  next_line++;

  fprintf(f, "%d and 4 %d -14 [4:0]\n", next_line, codes[3]);
  fprintf(f, "%d add 4 %d %d [10:0]\n", next_line + 1, next_line,
          b_immediate_loc - 10); // HACKY AF!!! Point on i[10:5] of b-type
  fprintf(f, "%d and 4 %d 14\n", next_line + 2, codes[3]);
  fprintf(f, "%d constd 4 11\n", next_line + 3);
  fprintf(f, "%d sll 4 %d %d [11]\n", next_line + 4, next_line + 2,
          next_line + 3);
  fprintf(f, "%d add 4 %d %d [11:0]\n", next_line + 5, next_line + 1,
          next_line + 4); // combine
  fprintf(f, "%d sll 4 %d 12 [14:12]\n", next_line + 6,
          codes[4]); // [14:12] is at funct3
  fprintf(f, "%d add 4 %d %d [14:0]\n", next_line + 7, next_line + 5,
          next_line + 6); // combine
  fprintf(f, "%d sll 4 %d 11 [19:15]\n", next_line + 8,
          codes[2]); // [19:15] is at rs1
  fprintf(f, "%d add 4 %d %d [19:0]\n", next_line + 9, next_line + 7,
          next_line + 8); // combine
  fprintf(f, "%d consth 4 fffff\n", next_line + 10);
  fprintf(f, "%d sra 4 %d %d\n", next_line + 11, command_loc,
          next_line + 10); // shift is way too much, but because arithmetic
                           // right shift now all bits ar equal to highest bit
  fprintf(f, "%d and 4 %d -%d [31:20]\n", next_line + 12, next_line + 11,
          next_line + 10);
  fprintf(f, "%d add 4 %d %d j-immediate\n", next_line + 13, next_line + 9,
          next_line + 12);
  int j_immediate_loc = next_line + 13;
  next_line += 14;

  fprintf(f, ";\n; sort opcodes to immediates\n");
  fprintf(f, "%d eq 1 %d %d\n", next_line, opcode_const_loc,
          codes[0]); // i opcode load
  fprintf(f, "%d eq 1 %d %d\n", next_line + 1, opcode_const_loc + 1,
          codes[0]); // i opcode math i
  fprintf(f, "%d eq 1 %d %d\n", next_line + 2, opcode_const_loc + 2,
          codes[0]); // u opcode auipc
  fprintf(f, "%d eq 1 %d %d\n", next_line + 3, opcode_const_loc + 3,
          codes[0]); // i opcode math wi
  fprintf(f, "%d eq 1 %d %d\n", next_line + 4, opcode_const_loc + 4,
          codes[0]); // s opcode store
  fprintf(f, "%d eq 1 %d %d\n", next_line + 5, opcode_const_loc + 5,
          codes[0]); //- opcode math reg
  fprintf(f, "%d eq 1 %d %d\n", next_line + 6, opcode_const_loc + 6,
          codes[0]); // u opcode lui
  fprintf(f, "%d eq 1 %d %d\n", next_line + 7, opcode_const_loc + 7,
          codes[0]); //- opcode math w
  fprintf(f, "%d eq 1 %d %d\n", next_line + 8, opcode_const_loc + 8,
          codes[0]); // b opcode branch
  fprintf(f, "%d eq 1 %d %d\n", next_line + 9, opcode_const_loc + 9,
          codes[0]); // i opcode jump r
  fprintf(f, "%d eq 1 %d %d\n", next_line + 10, opcode_const_loc + 10,
          codes[0]); // j opcode jump

  // i types: JALR(jump r), LOAD, math i, math wi
  fprintf(f, "%d or 1 %d %d\n", next_line + 11, next_line, next_line + 1);
  fprintf(f, "%d or 1 %d %d\n", next_line + 12, next_line + 11, next_line + 3);
  fprintf(f, "%d or 1 %d %d\n", next_line + 13, next_line + 12, next_line + 9);
  // u types: AUIPC, LUI
  fprintf(f, "%d or 1 %d %d\n", next_line + 14, next_line + 2, next_line + 6);

  int i_type_loc = next_line + 13;
  int s_type_loc = next_line + 4;
  int b_type_loc = next_line + 8;
  int u_type_loc = next_line + 14;
  int j_type_loc = next_line + 10;
  next_line += 15;

  // i type as default, can be used for shift amount shamt in S[L|R][L|A]I
  // commands
  fprintf(f, "%d ite 4 %d %d 14 i/r\n", next_line, i_type_loc,
          i_immediate_loc); // default to one to find errors. in random testing
                            // an i val of 1 is unexpected. in r-type, i is not
                            // used so it should have no impact
  fprintf(f, "%d ite 4 %d %d %d s\n", next_line + 1, s_type_loc,
          s_immediate_loc, next_line);
  fprintf(f, "%d ite 4 %d %d %d b\n", next_line + 2, b_type_loc,
          b_immediate_loc, next_line + 1);
  fprintf(f, "%d ite 4 %d %d %d u\n", next_line + 3, u_type_loc,
          u_immediate_loc, next_line + 2);
  fprintf(f, "%d ite 4 %d %d %d j\n", next_line + 4, j_type_loc,
          j_immediate_loc, next_line + 3);
  return next_line + 5;
}

int btor_check_4_all_commands(FILE *f, int next_line, int opcode_comp,
                              int *codes) {
  int constants_funct3 = next_line;
  fprintf(f, ";\n; constants for funct3\n");
  for (size_t i = 0; i < 8; i++) {
    fprintf(f, "%d constd 4 %ld\n", next_line, i);
    next_line++;
  }
  int constant_funct7 = next_line;
  fprintf(f, ";\n; Constant for funct7\n");
  fprintf(f, "%d constd 4 32\n", next_line); // second highest bit of funct7 set
  next_line++;

  int comp_funct3 = next_line;
  fprintf(f, ";\n; Compare current funct3\n");
  for (size_t i = 0; i < 8; i++) {
    fprintf(f, "%d eq 1 %d %ld\n", next_line, codes[4], constants_funct3 + i);
    next_line++;
  }

  fprintf(f, ";\n; Compare current funct7\n");
  fprintf(f, "%d and 4 %d %d\n", next_line, codes[5],
          constant_funct7); // and bitwise
  fprintf(f, "%d eq 1 %d %d funct7bit_set\n", next_line + 1, constant_funct7,
          next_line); // still the same -> funct7 bit set
  next_line += 2;
  int comp_funct7 = next_line - 1;

  int pre_comp = next_line;
  fprintf(f, ";\n; Some commands check against funct7, funct3 & opcode. They "
             "are pre-checked so we get an orderly list in the next step\n");
  fprintf(f, "%d and 1 -%d %d SRL(I)(W)_pre\n", next_line, comp_funct7,
          comp_funct3 + 5);
  fprintf(f, "%d and 1 %d %d SRA(I)(W)_pre\n", next_line + 1, comp_funct7,
          comp_funct3 + 5);
  fprintf(f, "%d and 1 -%d %d ADD(W)_pre\n", next_line + 2, comp_funct7,
          comp_funct3 + 0);
  fprintf(f, "%d and 1 %d %d SUB(W)_pre\n", next_line + 3, comp_funct7,
          comp_funct3 + 0);
  next_line += 4;

  fprintf(f, ";\n; Check all commands\n");
  // RV32I
  fprintf(f, "%d and 1 15 %d\n", next_line + 0, opcode_comp + 6);  // LUI
  fprintf(f, "%d and 1 15 %d\n", next_line + 1, opcode_comp + 2);  // AUIPC
  fprintf(f, "%d and 1 15 %d\n", next_line + 2, opcode_comp + 10); // JAL
  fprintf(f, "%d and 1 %d %d\n", next_line + 3, comp_funct3 + 0,
          opcode_comp + 9); // JALR

  fprintf(f, "%d and 1 %d %d\n", next_line + 4, comp_funct3 + 0,
          opcode_comp + 8); // BEQ
  fprintf(f, "%d and 1 %d %d\n", next_line + 5, comp_funct3 + 1,
          opcode_comp + 8); // BNE
  fprintf(f, "%d and 1 %d %d\n", next_line + 6, comp_funct3 + 4,
          opcode_comp + 8); // BLT
  fprintf(f, "%d and 1 %d %d\n", next_line + 7, comp_funct3 + 5,
          opcode_comp + 8); // BGE
  fprintf(f, "%d and 1 %d %d\n", next_line + 8, comp_funct3 + 6,
          opcode_comp + 8); // BLTU
  fprintf(f, "%d and 1 %d %d\n", next_line + 9, comp_funct3 + 7,
          opcode_comp + 8); // BGEU

  fprintf(f, "%d and 1 %d %d\n", next_line + 10, comp_funct3 + 0,
          opcode_comp + 0); // LB
  fprintf(f, "%d and 1 %d %d\n", next_line + 11, comp_funct3 + 1,
          opcode_comp + 0); // LH
  fprintf(f, "%d and 1 %d %d\n", next_line + 12, comp_funct3 + 2,
          opcode_comp + 0); // LW
  // LD comes in RV64I
  fprintf(f, "%d and 1 %d %d\n", next_line + 13, comp_funct3 + 4,
          opcode_comp + 0); // LBU
  fprintf(f, "%d and 1 %d %d\n", next_line + 14, comp_funct3 + 5,
          opcode_comp + 0); // LHU
  // LWU comes in RV64I

  fprintf(f, "%d and 1 %d %d\n", next_line + 15, comp_funct3 + 0,
          opcode_comp + 4); // SB
  fprintf(f, "%d and 1 %d %d\n", next_line + 16, comp_funct3 + 1,
          opcode_comp + 4); // SH
  fprintf(f, "%d and 1 %d %d\n", next_line + 17, comp_funct3 + 2,
          opcode_comp + 4); // SW
  // SD comes in RV64I

  fprintf(f, "%d and 1 %d %d\n", next_line + 18, comp_funct3 + 0,
          opcode_comp + 1); // ADDI
  // SUBI doesnt exist
  // SLLI is overwritten by RV64I
  fprintf(f, "%d and 1 %d %d\n", next_line + 19, comp_funct3 + 2,
          opcode_comp + 1); // SLTI
  fprintf(f, "%d and 1 %d %d\n", next_line + 20, comp_funct3 + 3,
          opcode_comp + 1); // SLTIU
  fprintf(f, "%d and 1 %d %d\n", next_line + 21, comp_funct3 + 4,
          opcode_comp + 1); // XORI
  // SRLI is overwritten by RV64I
  // SRAI is overwritten by RV64I
  fprintf(f, "%d and 1 %d %d\n", next_line + 22, comp_funct3 + 6,
          opcode_comp + 1); // ORI
  fprintf(f, "%d and 1 %d %d\n", next_line + 23, comp_funct3 + 7,
          opcode_comp + 1); // ANDI

  fprintf(f, "%d and 1 %d %d\n", next_line + 24, pre_comp + 2,
          opcode_comp + 5); // ADD; S(L|R)(L|A)I is overwritten by RV64I
  fprintf(f, "%d and 1 %d %d\n", next_line + 25, pre_comp + 3,
          opcode_comp + 5); // SUB
  fprintf(f, "%d and 1 %d %d\n", next_line + 26, comp_funct3 + 1,
          opcode_comp + 5); // SLL
  fprintf(f, "%d and 1 %d %d\n", next_line + 27, comp_funct3 + 2,
          opcode_comp + 5); // SLT
  fprintf(f, "%d and 1 %d %d\n", next_line + 28, comp_funct3 + 3,
          opcode_comp + 5); // SLTU
  fprintf(f, "%d and 1 %d %d\n", next_line + 29, comp_funct3 + 4,
          opcode_comp + 5); // XOR
  fprintf(f, "%d and 1 %d %d\n", next_line + 30, pre_comp + 0,
          opcode_comp + 5); // SRL
  fprintf(f, "%d and 1 %d %d\n", next_line + 31, pre_comp + 1,
          opcode_comp + 5); // SRA
  fprintf(f, "%d and 1 %d %d\n", next_line + 32, comp_funct3 + 6,
          opcode_comp + 5); // OR
  fprintf(f, "%d and 1 %d %d\n", next_line + 33, comp_funct3 + 7,
          opcode_comp + 5); // AND

  // RV64I
  fprintf(f, "%d and 1 %d %d\n", next_line + 34, comp_funct3 + 6,
          opcode_comp + 0); // LWU
  fprintf(f, "%d and 1 %d %d\n", next_line + 35, comp_funct3 + 3,
          opcode_comp + 0); // LD

  fprintf(f, "%d and 1 %d %d\n", next_line + 36, comp_funct3 + 3,
          opcode_comp + 4); // SD

  fprintf(f, "%d and 1 %d %d\n", next_line + 37, comp_funct3 + 1,
          opcode_comp + 1); // SLLI
  fprintf(f, "%d and 1 %d %d\n", next_line + 38, pre_comp + 0,
          opcode_comp + 1); // SRLI
  fprintf(f, "%d and 1 %d %d\n", next_line + 39, pre_comp + 1,
          opcode_comp + 1); // SRAI

  fprintf(f, "%d and 1 %d %d\n", next_line + 40, comp_funct3 + 0,
          opcode_comp + 3); // ADDIW
  fprintf(f, "%d and 1 %d %d\n", next_line + 41, comp_funct3 + 1,
          opcode_comp + 3); // SLLIW
  fprintf(f, "%d and 1 %d %d\n", next_line + 42, pre_comp + 0,
          opcode_comp + 3); // SRLIW
  fprintf(f, "%d and 1 %d %d\n", next_line + 43, pre_comp + 1,
          opcode_comp + 3); // SRAIW

  fprintf(f, "%d and 1 %d %d\n", next_line + 44, pre_comp + 2,
          opcode_comp + 7); // ADDW
  fprintf(f, "%d and 1 %d %d\n", next_line + 45, pre_comp + 3,
          opcode_comp + 7); // SUBW
  fprintf(f, "%d and 1 %d %d\n", next_line + 46, comp_funct3 + 1,
          opcode_comp + 7); // SLLW
  fprintf(f, "%d and 1 %d %d\n", next_line + 47, pre_comp + 0,
          opcode_comp + 7); // SRLW
  fprintf(f, "%d and 1 %d %d\n", next_line + 48, pre_comp + 1,
          opcode_comp + 7); // SRAW
  return next_line + 49;
}

int btor_updates(FILE *f, int next_line, int register_loc, int memory_loc,
                 int command_check_loc, int immediate_loc, int opcode_comp,
                 int *codes, int reg_init_flag_loc) {
  fprintf(f, ";\n; Next Functions for Registers and Memory\n");
  int comparison_constants_loc = next_line;
  fprintf(f, "; Get rs1, rs2 values\n");
  for (size_t i = 0; i < 33; i++) {
    fprintf(f, "%d constd 5 %ld\n", next_line, i);
    next_line++;
  }
  fprintf(f, "%d uext 5 %d 32 rs1_code_ext\n", next_line, codes[2]);
  fprintf(f, "%d uext 5 %d 32 rs2_code_ext\n", next_line + 1, codes[3]);
  fprintf(f, "%d uext 5 %d 32 rd_code_ext\n", next_line + 2, codes[1]);
  int rs1_code_ext = next_line;
  int rs2_code_ext = next_line + 1;
  int rd_code_ext = next_line + 2;
  next_line += 3;

  int rs1_comp_loc = next_line;
  for (size_t i = 1; i < 32; i++) {
    fprintf(f, "%d eq 1 %ld %d\n", next_line, comparison_constants_loc + i,
            rs1_code_ext);
    next_line++;
  }
  int rs2_comp_loc = next_line;
  for (size_t i = 1; i < 32; i++) {
    fprintf(f, "%d eq 1 %ld %d\n", next_line, comparison_constants_loc + i,
            rs2_code_ext);
    next_line++;
  }

  fprintf(f, "%d ite 5 %d %d %d rs1_x1_x0\n", next_line, rs1_comp_loc,
          register_loc + 1, register_loc + 0);
  next_line++;
  for (size_t i = 2; i < 32; i++) {
    fprintf(f, "%d ite 5 %ld %ld %d rs1_x%ld\n", next_line,
            rs1_comp_loc + i - 1, register_loc + i, next_line - 1, i);
    next_line++;
  }
  int rs1_val_loc = next_line - 1;

  fprintf(f, "%d ite 5 %d %d %d rs2_x1_x0\n", next_line, rs2_comp_loc,
          register_loc + 1, register_loc + 0);
  next_line++;
  for (size_t i = 2; i < 32; i++) {
    fprintf(f, "%d ite 5 %ld %ld %d rs2_x%ld\n", next_line,
            rs2_comp_loc + i - 1, register_loc + i, next_line - 1, i);
    next_line++;
  }
  int rs2_val_loc = next_line - 1;

  fprintf(f, ";\n; Calculating values for commands\n");
  fprintf(f, ";\n; Flow Control\n");
  fprintf(f, "%d uext 5 %d %d pc_val_64bit\n", next_line, register_loc + 32,
          64 - BTOR_MEMORY_SIZE);
  fprintf(f, "%d sext 5 %d 32 immediate_64bit\n", next_line + 1, immediate_loc);
  fprintf(f, "%d add 5 %d %d auipc_rd\n", next_line + 2, next_line,
          next_line + 1);
  int pc_64bit = next_line;
  int immediate_64bit = next_line + 1;
  int pc_immediate_added =
      next_line +
      2; // as immediate is opcode sensitive, this holds for all commands
  int lui_rd = immediate_64bit; // is also sign extended immediate!
  int auipc_rd = pc_immediate_added;
  next_line += 3;

  fprintf(f, "%d slice 2 %d %d 0 jal_pc\n", next_line, pc_immediate_added,
          BTOR_MEMORY_SIZE - 1);
  int jal_pc = next_line;
  next_line++;

  fprintf(f, "%d constd 5 4\n", next_line);
  fprintf(f, "%d add 5 %d %d\n", next_line + 1, pc_64bit, next_line);
  int jal_rd = next_line + 1; // pc + 4
  next_line += 2;

  fprintf(f, "%d add 5 %d %d\n", next_line, rs1_val_loc, immediate_64bit);
  fprintf(f, "%d and 5 %d -%d\n", next_line + 1, next_line,
          comparison_constants_loc + 1);
  fprintf(f, "%d slice 2 %d %d 0\n", next_line + 2, next_line + 1,
          BTOR_MEMORY_SIZE - 1);
  next_line += 3;
  int jalr_pc = next_line - 1;
  int jalr_rd = jal_rd; // JALR rd is the same as JAL rd

  int branch_pc_true = jal_pc; // as immediate is command sensitive, this also
                               // works for b-type encoding

  fprintf(f, "%d slice 2 %d %d 0\n", next_line, jal_rd, BTOR_MEMORY_SIZE - 1);
  int branch_pc_false = next_line;
  next_line++;

  fprintf(f, ";\n; Branch Comparisons\n");
  fprintf(f, "%d eq 1 %d %d\n", next_line, rs1_val_loc, rs2_val_loc); // BEQ
  fprintf(f, "%d neq 1 %d %d\n", next_line + 1, rs1_val_loc,
          rs2_val_loc); // BNE
  fprintf(f, "%d slt 1 %d %d\n", next_line + 2, rs1_val_loc,
          rs2_val_loc); // BLT
  fprintf(f, "%d sgte 1 %d %d\n", next_line + 3, rs1_val_loc,
          rs2_val_loc); // BGE
  fprintf(f, "%d ult 1 %d %d\n", next_line + 4, rs1_val_loc,
          rs2_val_loc); // BLTU
  fprintf(f, "%d ugte 1 %d %d\n", next_line + 5, rs1_val_loc,
          rs2_val_loc); // BGEU
  int beq_check = next_line;
  int bne_check = next_line + 1;
  int blt_check = next_line + 2;
  int bge_check = next_line + 3;
  int bltu_check = next_line + 4;
  int bgeu_check = next_line + 5;
  next_line += 6;

  // LOAD
  fprintf(f, ";\n; LOAD\n");

  fprintf(f, "%d sort bitvec 16\n", next_line);
  int size16_loc = next_line;
  next_line++;

  fprintf(f, "%d add 5 %d %d\n", next_line, rs1_val_loc, immediate_64bit);
  int load_address = next_line;
  next_line++;

  int rs1_added = next_line;
  for (size_t i = 0; i < 8; i++) {
    fprintf(f, "%d add 5 %d %ld\n", next_line, load_address,
            comparison_constants_loc + i);
    next_line++;
  }
  int rs1_added_shortened = next_line;
  for (size_t i = 0; i < 8; i++) {
    fprintf(f, "%d slice 2 %ld %d 0\n", next_line, rs1_added + i,
            BTOR_MEMORY_SIZE - 1);
    next_line++;
  }

  int read_cells = next_line;
  for (size_t i = 0; i < 8; i++) {
    fprintf(f, "%d read 3 %d %ld\n", next_line, memory_loc,
            rs1_added_shortened + i);
    next_line++;
  }
  int lb_rd = next_line;
  fprintf(f, "%d sext 5 %d 56 lb_rd\n", next_line, read_cells + 0);
  next_line++;

  fprintf(f, "%d concat %d %d %d\n", next_line, size16_loc, read_cells + 1,
          read_cells);
  fprintf(f, "%d sext 5 %d 48 lh_rd\n", next_line + 1, next_line);
  int lh_rd = next_line + 1;
  next_line += 2;

  fprintf(f, "%d concat %d %d %d\n", next_line, size16_loc, read_cells + 3,
          read_cells + 2);
  fprintf(f, "%d concat 4 %d %d\n", next_line + 1, next_line, lh_rd - 1);
  fprintf(f, "%d sext 5 %d 32 lw_rd\n", next_line + 2, next_line + 1);
  int lw_rd = next_line + 2;
  next_line += 3;

  fprintf(f, "%d concat %d %d %d\n", next_line, size16_loc, read_cells + 5,
          read_cells + 4);
  fprintf(f, "%d concat %d %d %d\n", next_line + 1, size16_loc, read_cells + 7,
          read_cells + 6);
  fprintf(f, "%d concat 4 %d %d\n", next_line + 2, next_line + 1, next_line);
  fprintf(f, "%d concat 5 %d %d ld_rd\n", next_line + 3, next_line + 2,
          lw_rd - 1);
  int ld_rd = next_line + 3;
  next_line += 4;

  fprintf(f, "%d uext 5 %d 56 lbu\n", next_line, read_cells + 0);
  int lbu_rd = next_line;
  next_line++;

  fprintf(f, "%d uext 5 %d 48 lhu\n", next_line, lh_rd - 1);
  int lhu_rd = next_line;
  next_line++;

  fprintf(f, "%d uext 5 %d 32 lwu_rd\n", next_line, lw_rd - 1);
  int lwu_rd = next_line;
  next_line++;

  // STORE
  fprintf(f, ";\n; STORE\n");
  int store_memory_bytes = next_line;
  fprintf(f, "%d slice 3 %d 7 0\n", next_line, rs2_val_loc); // Store byte 1
  fprintf(f, "%d slice 3 %d 15 8\n", next_line + 1,
          rs2_val_loc); // Store byte 2
  fprintf(f, "%d slice 3 %d 23 16\n", next_line + 2,
          rs2_val_loc); // Store byte 3
  fprintf(f, "%d slice 3 %d 31 24\n", next_line + 3,
          rs2_val_loc); // Store byte 4
  fprintf(f, "%d slice 3 %d 39 32\n", next_line + 4,
          rs2_val_loc); // Store byte 5
  fprintf(f, "%d slice 3 %d 47 40\n", next_line + 5,
          rs2_val_loc); // Store byte 6
  fprintf(f, "%d slice 3 %d 55 48\n", next_line + 6,
          rs2_val_loc); // Store byte 7
  fprintf(f, "%d slice 3 %d 63 56\n", next_line + 7,
          rs2_val_loc); // Store byte 8
  next_line += 8;

  int mem_address_consts_loc = next_line;
  fprintf(f, "%d one 2\n", next_line);
  next_line++;

  fprintf(f, "%d add 5 %d %d mem_adress_uncut\n", next_line, rs1_val_loc,
          immediate_64bit); // Add rs1 value to immediate
  fprintf(f, "%d slice 2 %d %d 0 mem_address\n", next_line + 1, next_line,
          BTOR_MEMORY_SIZE - 1); // Cut to BTOR memory size
  int mem_address_cut = next_line + 1;
  next_line += 2;
  for (size_t i = 0; i < 8; i++) {
    fprintf(f, "%d add 2 %d %d mem_address+%ld\n", next_line, next_line - 1,
            mem_address_consts_loc, i);
    next_line++;
  }

  fprintf(f, "%d write 6 %d %d %d sb\n", next_line, memory_loc, mem_address_cut,
          store_memory_bytes); // Store byte 1
  fprintf(f, "%d write 6 %d %d %d sh\n", next_line + 1, next_line,
          mem_address_cut + 1, store_memory_bytes + 1); // Store byte 2
  fprintf(f, "%d write 6 %d %d %d\n", next_line + 2, next_line + 1,
          mem_address_cut + 2, store_memory_bytes + 2); // Store byte 3
  fprintf(f, "%d write 6 %d %d %d sw\n", next_line + 3, next_line + 2,
          mem_address_cut + 3, store_memory_bytes + 3); // Store byte 4
  fprintf(f, "%d write 6 %d %d %d\n", next_line + 4, next_line + 3,
          mem_address_cut + 4, store_memory_bytes + 4); // Store byte 5
  fprintf(f, "%d write 6 %d %d %d\n", next_line + 5, next_line + 4,
          mem_address_cut + 5, store_memory_bytes + 5); // Store byte 6
  fprintf(f, "%d write 6 %d %d %d\n", next_line + 6, next_line + 5,
          mem_address_cut + 6, store_memory_bytes + 6); // Store byte 7
  fprintf(f, "%d write 6 %d %d %d sd\n", next_line + 7, next_line + 6,
          mem_address_cut + 7, store_memory_bytes + 7); // Store byte 8
  int sb_mem = next_line;
  int sh_mem = next_line + 1;
  int sw_mem = next_line + 3;
  int sd_mem = next_line + 7;
  next_line += 8;

  // MATH i
  fprintf(f, ";\n; MATH immediate\n");
  int math_i_rd_addi = next_line;
  fprintf(f, "%d add 5 %d %d addi_rd\n", next_line, rs1_val_loc,
          immediate_64bit); // ADDI
  next_line++;

  int immediate_6bit_shamt = next_line + 1;
  fprintf(f, "%d consth 5 3f\n", next_line);
  fprintf(f, "%d and 5 %d %d\n", next_line + 1, immediate_64bit, next_line);
  next_line += 2;

  int math_i_rd_slli = next_line;
  fprintf(f, "%d sll 5 %d %d slli_rd\n", next_line, rs1_val_loc,
          immediate_6bit_shamt); // SLLI
  next_line++;

  int math_i_rd_slti = next_line + 1;
  fprintf(f, "%d slt 1 %d %d slti_rd\n", next_line, rs1_val_loc,
          immediate_64bit);
  fprintf(f, "%d uext 5 %d 63 slti_rd\n", next_line + 1, next_line); // SLTI
  next_line += 2;

  int math_i_rd_sltiu = next_line + 1;
  fprintf(f, "%d ult 1 %d %d sltiu_rd\n", next_line, rs1_val_loc,
          immediate_64bit);
  fprintf(f, "%d uext 5 %d 63 sltiu_rd\n", next_line + 1, next_line); // SLTIU
  next_line += 2;

  int math_i_rd_xori = next_line;
  fprintf(f, "%d xor 5 %d %d xori_rd\n", next_line, rs1_val_loc,
          immediate_64bit); // XORI
  next_line++;

  int math_i_rd_srli = next_line;
  fprintf(f, "%d srl 5 %d %d srli_rd\n", next_line, rs1_val_loc,
          immediate_6bit_shamt); // SRLI
  next_line++;

  int math_i_rd_srai = next_line + 1;
  fprintf(f, "%d sub 5 %d %d srai_rd\n", next_line, immediate_64bit,
          comparison_constants_loc + 32); // -32 removes the bit in funct7 wich
                                          // differentiates SRAI from SRLI
  fprintf(f, "%d sra 5 %d %d srai_rd\n", next_line + 1, rs1_val_loc,
          immediate_6bit_shamt); // SRAI
  next_line += 2;

  int math_i_rd_ori = next_line;
  fprintf(f, "%d or 5 %d %d ori_rd\n", next_line, rs1_val_loc,
          immediate_64bit); // ORI
  next_line++;

  int math_i_rd_andi = next_line;
  fprintf(f, "%d and 5 %d %d andi_rd\n", next_line, rs1_val_loc,
          immediate_64bit); // ANDI
  next_line++;

  // MATH reg
  fprintf(f, ";\n; MATH Register based\n");
  int math_reg_rd_add = next_line;
  fprintf(f, "%d add 5 %d %d add_rd\n", next_line, rs1_val_loc,
          rs2_val_loc); // ADD
  next_line++;

  int math_reg_rd_sub = next_line;
  fprintf(f, "%d sub 5 %d %d sub_rd\n", next_line, rs1_val_loc,
          rs2_val_loc); // SUB
  next_line++;

  fprintf(f, "%d consth 5 3f\n", next_line);
  fprintf(f, "%d and 5 %d %d\n", next_line + 1, rs2_val_loc, next_line);
  int rs2_shamt_loc =
      next_line + 1; // rs2 shamt is the lower 6 bits of rs2_val_cut_loc
  next_line += 2;

  int math_reg_rd_sll = next_line;
  fprintf(f, "%d sll 5 %d %d sll_rd\n", next_line, rs1_val_loc,
          rs2_shamt_loc); // SLL
  next_line++;

  int math_reg_rd_slt = next_line + 1;
  fprintf(f, "%d slt 1 %d %d slt_rd\n", next_line, rs1_val_loc, rs2_val_loc);
  fprintf(f, "%d uext 5 %d 63 slt_rd\n", next_line + 1, next_line); // SLT
  next_line += 2;

  int math_reg_rd_sltu = next_line + 1;
  fprintf(f, "%d ult 1 %d %d sltu_rd\n", next_line, rs1_val_loc, rs2_val_loc);
  fprintf(f, "%d uext 5 %d 63 sltu_rd\n", next_line + 1, next_line); // SLTU
  next_line += 2;

  int math_reg_rd_xor = next_line;
  fprintf(f, "%d xor 5 %d %d xor_rd\n", next_line, rs1_val_loc,
          rs2_val_loc); // XOR
  next_line++;

  int math_reg_rd_srl = next_line;
  fprintf(f, "%d srl 5 %d %d srl_rd\n", next_line, rs1_val_loc,
          rs2_shamt_loc); // SRL
  next_line++;

  int math_reg_rd_sra = next_line;
  fprintf(f, "%d sra 5 %d %d sra_rd\n", next_line, rs1_val_loc,
          rs2_shamt_loc); // SRA
  next_line++;

  int math_reg_rd_or = next_line;
  fprintf(f, "%d or 5 %d %d or_rd\n", next_line, rs1_val_loc,
          rs2_val_loc); // OR
  next_line++;

  int math_reg_rd_and = next_line;
  fprintf(f, "%d and 5 %d %d and_rd\n", next_line, rs1_val_loc,
          rs2_val_loc); // AND
  next_line++;

  // MATH WI
  fprintf(f, ";\n; MATH Word Immediate\n");

  fprintf(f, "%d slice 4 %d 31 0\n", next_line,
          rs1_val_loc); // rs1 cut to 32 bit
  int rs1_val_cut_loc = next_line;
  next_line++;

  fprintf(f, "%d add 4 %d %d\n", next_line, rs1_val_cut_loc,
          immediate_loc); // ADDIW
  fprintf(f, "%d sext 5 %d 32 addiw_rd\n", next_line + 1, next_line);
  int math_iw_rd_addiw = next_line + 1;
  next_line += 2;

  fprintf(f, "%d sll 4 %d %d\n", next_line, rs1_val_cut_loc,
          codes[3]); // SLLIW, shamt is exactly at the place of rs2 encoding
  fprintf(f, "%d sext 5 %d 32 slliw_rd\n", next_line + 1, next_line);
  int math_iw_rd_slliw = next_line + 1;
  next_line += 2;

  fprintf(f, "%d srl 4 %d %d\n", next_line, rs1_val_cut_loc, codes[3]); // SRLIW
  fprintf(f, "%d sext 5 %d 32 srliw_rd\n", next_line + 1, next_line);
  int math_iw_rd_srliw = next_line + 1;
  next_line += 2;

  fprintf(f, "%d sra 4 %d %d\n", next_line, rs1_val_cut_loc, codes[3]); // SRAIW
  fprintf(f, "%d sext 5 %d 32 sraiw_rd\n", next_line + 1, next_line);
  int math_iw_rd_sraiw = next_line + 1;
  next_line += 2;

  fprintf(f, ";\n; MATH Word\n");

  fprintf(f, "%d slice 4 %d 31 0\n", next_line,
          rs2_val_loc); // rs2 cut to 32 bit
  int rs2_val_cut_loc = next_line;
  next_line++;

  fprintf(f, "%d add 4 %d %d\n", next_line, rs1_val_cut_loc,
          rs2_val_cut_loc); // ADDW
  fprintf(f, "%d sext 5 %d 32 addw_rd\n", next_line + 1, next_line);
  int math_w_rd_addw = next_line + 1;
  next_line += 2;

  fprintf(f, "%d sub 4 %d %d\n", next_line, rs1_val_cut_loc,
          rs2_val_cut_loc); // SUBW
  fprintf(f, "%d sext 5 %d 32 subw_rd\n", next_line + 1, next_line);
  int math_w_rd_subw = next_line + 1;
  next_line += 2;

  fprintf(f, "%d consth 4 1f\n", next_line);
  fprintf(f, "%d and 4 %d %d\n", next_line + 1, rs2_val_cut_loc, next_line);
  int rs2_w_shamt_loc =
      next_line + 1; // rs2 shamt is the lower 6 bits of rs2_val_cut_loc
  next_line += 2;

  fprintf(f, "%d sll 4 %d %d\n", next_line, rs1_val_cut_loc,
          rs2_w_shamt_loc); // SLLW
  fprintf(f, "%d sext 5 %d 32 sllw_rd\n", next_line + 1, next_line);
  int math_w_rd_sllw = next_line + 1;
  next_line += 2;

  fprintf(f, "%d srl 4 %d %d\n", next_line, rs1_val_cut_loc,
          rs2_w_shamt_loc); // SRLW
  fprintf(f, "%d sext 5 %d 32 srlw_rd\n", next_line + 1, next_line);
  int math_w_rd_srlw = next_line + 1;
  next_line += 2;

  fprintf(f, "%d sra 4 %d %d\n", next_line, rs1_val_cut_loc,
          rs2_w_shamt_loc); // SRAW
  fprintf(f, "%d sext 5 %d 32 sraw_rd\n", next_line + 1, next_line);
  int math_w_rd_sraw = next_line + 1;
  next_line += 2;

  fprintf(f, ";\n; Update register x0\n");
  fprintf(f, "%d next 5 %d %d x0_new\n", next_line, register_loc + 0,
          register_loc + 0);
  next_line++;
  fprintf(f, "; Update x0 flag\n");
  fprintf(f, "%d next 1 %d 15 x0_always_initialised\n", next_line,
          reg_init_flag_loc);
  next_line++;
  for (size_t i = 1; i < 32; i++) {
    fprintf(f, ";\n; Update register x%ld\n", i);
    fprintf(f, "%d eq 1 %ld %d x%ld_is_rd\n", next_line,
            comparison_constants_loc + i, rd_code_ext, i);

    fprintf(f, "%d ite 5 %d %d %ld x%ld_lui\n", next_line + 1,
            command_check_loc, lui_rd, register_loc + i,
            i); // command without rd
    fprintf(f, "%d ite 5 %d %d %d x%ld_auipc\n", next_line + 2,
            command_check_loc + 1, auipc_rd, next_line + 1, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_jal\n", next_line + 3,
            command_check_loc + 2, jal_rd, next_line + 2, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_jalr\n", next_line + 4,
            command_check_loc + 3, jalr_rd, next_line + 3, i);

    fprintf(f, "%d ite 5 %d %d %d x%ld_lb\n", next_line + 5,
            command_check_loc + 10, lb_rd, next_line + 4, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_lh\n", next_line + 6,
            command_check_loc + 11, lh_rd, next_line + 5, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_lw\n", next_line + 7,
            command_check_loc + 12, lw_rd, next_line + 6, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_ld\n", next_line + 8,
            command_check_loc + 35, ld_rd, next_line + 7, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_lbu\n", next_line + 9,
            command_check_loc + 13, lbu_rd, next_line + 8, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_lhu\n", next_line + 10,
            command_check_loc + 14, lhu_rd, next_line + 9, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_lwu\n", next_line + 11,
            command_check_loc + 34, lwu_rd, next_line + 10, i);

    fprintf(f, "%d ite 5 %d %d %d x%ld_addi\n", next_line + 12,
            command_check_loc + 18, math_i_rd_addi, next_line + 11, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_slli\n", next_line + 13,
            command_check_loc + 37, math_i_rd_slli, next_line + 12, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_slti\n", next_line + 14,
            command_check_loc + 19, math_i_rd_slti, next_line + 13, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sltiu\n", next_line + 15,
            command_check_loc + 20, math_i_rd_sltiu, next_line + 14, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_xori\n", next_line + 16,
            command_check_loc + 21, math_i_rd_xori, next_line + 15, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_srli\n", next_line + 17,
            command_check_loc + 38, math_i_rd_srli, next_line + 16, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_srai\n", next_line + 18,
            command_check_loc + 39, math_i_rd_srai, next_line + 17, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_ori\n", next_line + 19,
            command_check_loc + 22, math_i_rd_ori, next_line + 18, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_andi\n", next_line + 20,
            command_check_loc + 23, math_i_rd_andi, next_line + 19, i);

    fprintf(f, "%d ite 5 %d %d %d x%ld_add\n", next_line + 21,
            command_check_loc + 24, math_reg_rd_add, next_line + 20, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sub\n", next_line + 22,
            command_check_loc + 25, math_reg_rd_sub, next_line + 21, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sll\n", next_line + 23,
            command_check_loc + 26, math_reg_rd_sll, next_line + 22, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_slt\n", next_line + 24,
            command_check_loc + 27, math_reg_rd_slt, next_line + 23, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sltu\n", next_line + 25,
            command_check_loc + 28, math_reg_rd_sltu, next_line + 24, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_xor\n", next_line + 26,
            command_check_loc + 29, math_reg_rd_xor, next_line + 25, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_srl\n", next_line + 27,
            command_check_loc + 30, math_reg_rd_srl, next_line + 26, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sra\n", next_line + 28,
            command_check_loc + 31, math_reg_rd_sra, next_line + 27, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_or\n", next_line + 29,
            command_check_loc + 32, math_reg_rd_or, next_line + 28, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_and\n", next_line + 30,
            command_check_loc + 33, math_reg_rd_and, next_line + 29, i);

    fprintf(f, "%d ite 5 %d %d %d x%ld_addiw\n", next_line + 31,
            command_check_loc + 40, math_iw_rd_addiw, next_line + 30, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_slliw\n", next_line + 32,
            command_check_loc + 41, math_iw_rd_slliw, next_line + 31, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_srliw\n", next_line + 33,
            command_check_loc + 42, math_iw_rd_srliw, next_line + 32, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sraiw\n", next_line + 34,
            command_check_loc + 43, math_iw_rd_sraiw, next_line + 33, i);

    fprintf(f, "%d ite 5 %d %d %d x%ld_addw\n", next_line + 35,
            command_check_loc + 44, math_w_rd_addw, next_line + 34, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_subw\n", next_line + 36,
            command_check_loc + 45, math_w_rd_subw, next_line + 35, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sllw\n", next_line + 37,
            command_check_loc + 46, math_w_rd_sllw, next_line + 36, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_srlw\n", next_line + 38,
            command_check_loc + 47, math_w_rd_srlw, next_line + 37, i);
    fprintf(f, "%d ite 5 %d %d %d x%ld_sraw\n", next_line + 39,
            command_check_loc + 48, math_w_rd_sraw, next_line + 38, i);

    fprintf(f, "%d ite 5 %d %d %ld x%ld_new\n", next_line + 40, next_line,
            next_line + 39, register_loc + i, i); // check if xi is rd
    fprintf(f, "%d next 5 %ld %d x%ld_new\n", next_line + 41, register_loc + i,
            next_line + 40, i);
    fprintf(f, ";Also update init-flag\n");
    // Test if command is Branch or Store
    fprintf(f, "%d or 1 %d %d opcode_is_branch_store\n", next_line + 42,
            opcode_comp + 4, opcode_comp + 8);
    fprintf(f, "%d ite 1 -%d 15 %ld command_check\n", next_line + 43,
            next_line + 42,
            reg_init_flag_loc + i); // only if not branch or store
    fprintf(f, "%d ite 1 %d %d %ld rd_check\n", next_line + 44, next_line,
            next_line + 43, reg_init_flag_loc + i);
    fprintf(f, "%d next 1 %ld %d reg_init_flag_new\n", next_line + 45,
            reg_init_flag_loc + i, next_line + 44);

    next_line += 46;
  }
  fprintf(f, ";\n; Update PC\n");
  fprintf(f, "%d ite 2 %d %d %d pc_beq_decider\n", next_line, beq_check,
          branch_pc_true, branch_pc_false);
  fprintf(f, "%d ite 2 %d %d %d pc_bne_decider\n", next_line + 1, bne_check,
          branch_pc_true, branch_pc_false);
  fprintf(f, "%d ite 2 %d %d %d pc_blt_decider\n", next_line + 2, blt_check,
          branch_pc_true, branch_pc_false);
  fprintf(f, "%d ite 2 %d %d %d pc_bge_decider\n", next_line + 3, bge_check,
          branch_pc_true, branch_pc_false);
  fprintf(f, "%d ite 2 %d %d %d pc_bltu_decider\n", next_line + 4, bltu_check,
          branch_pc_true, branch_pc_false);
  fprintf(f, "%d ite 2 %d %d %d pc_bgeu_decider\n", next_line + 5, bgeu_check,
          branch_pc_true, branch_pc_false);

  fprintf(f, "%d ite 2 %d %d %d pc_jal\n", next_line + 6, command_check_loc + 2,
          jal_pc, branch_pc_false);
  fprintf(f, "%d ite 2 %d %d %d pc_jalr\n", next_line + 7,
          command_check_loc + 3, jalr_pc, next_line + 6);

  fprintf(f, "%d ite 2 %d %d %d pc_beq\n", next_line + 8, command_check_loc + 4,
          next_line, next_line + 7);
  fprintf(f, "%d ite 2 %d %d %d pc_bne\n", next_line + 9, command_check_loc + 5,
          next_line + 1, next_line + 8);
  fprintf(f, "%d ite 2 %d %d %d pc_blt\n", next_line + 10,
          command_check_loc + 6, next_line + 2, next_line + 9);
  fprintf(f, "%d ite 2 %d %d %d pc_bge\n", next_line + 11,
          command_check_loc + 7, next_line + 3, next_line + 10);
  fprintf(f, "%d ite 2 %d %d %d pc_bltu\n", next_line + 12,
          command_check_loc + 8, next_line + 4, next_line + 11);
  fprintf(f, "%d ite 2 %d %d %d pc_bgeu\n", next_line + 13,
          command_check_loc + 9, next_line + 5, next_line + 12);

  fprintf(f, "%d next 2 %d %d pc_new\n", next_line + 14, register_loc + 32,
          next_line + 13);
  next_line += 15;

  fprintf(f, ";\n; Update memory\n");
  fprintf(f, "%d ite 6 %d %d %d mem_sb\n", next_line, command_check_loc + 15,
          sb_mem, memory_loc);
  fprintf(f, "%d ite 6 %d %d %d mem_sh\n", next_line + 1,
          command_check_loc + 16, sh_mem, next_line);
  fprintf(f, "%d ite 6 %d %d %d mem_sw\n", next_line + 2,
          command_check_loc + 17, sw_mem, next_line + 1);
  fprintf(f, "%d ite 6 %d %d %d mem_sd\n", next_line + 3,
          command_check_loc + 36, sd_mem, next_line + 2);
  fprintf(f, "%d next 6 %d %d memory_new\n", next_line + 4, memory_loc,
          next_line + 3);
  next_line += 5;

  fprintf(f, ";\n; Some little helpers for bad command detection\n");
  fprintf(f, "; misaligned instruction fetch error\n");
  fprintf(f, "%d consth 2 3\n", next_line);
  fprintf(f, "%d zero 2\n", next_line + 1);
  int pc_consts_loc = next_line;
  int pc_zero = next_line + 1;
  next_line += 2;
  fprintf(f, "%d and 2 %d %d\n", next_line, pc_consts_loc, jal_pc);
  fprintf(f, "%d and 2 %d %d\n", next_line + 1, pc_consts_loc, jalr_pc);

  fprintf(f, "%d neq 1 %d %d misaligned_jal_pc\n", next_line + 2, next_line,
          pc_zero); // error
  fprintf(f, "%d neq 1 %d %d misaligned_jalr_pc\n", next_line + 3,
          next_line + 1, pc_zero); // error

  fprintf(f, "%d and 1 %d %d mis_and_jal\n", next_line + 4,
          command_check_loc + 2, next_line + 2);
  fprintf(f, "%d and 1 %d %d mis_and_jalr\n", next_line + 5,
          command_check_loc + 3, next_line + 3);

  fprintf(f, "%d or 1 %d %d\n", next_line + 6, next_line + 4,
          next_line + 5); // option jal or jalr
  next_line += 7;

  return next_line;
}

int btor_bad_counter(FILE *f, int next_line, int counter_loc,
                     int counterlimit) {
  fprintf(f, ";\n; Bad counter\n");
  fprintf(f, "%d constd 4 %d\n", next_line, counterlimit);
  fprintf(f, "%d eq 1 %d %d\n", next_line + 1, counter_loc,
          next_line); // Check if counter is equal to limit
  fprintf(f, "%d bad %d counter_maxed\n", next_line + 2, next_line + 1);
  return next_line + 3;
}
int btor_bad_command(FILE *f, int next_line, int command_loc, int opcode_comp,
                     int badstate_pretest) {
  fprintf(f, ";\n; Bad opcode\n");
  fprintf(f, "%d or 1 %d %d\n", next_line, opcode_comp, opcode_comp + 1);
  next_line++;
  for (int i = 2; i < 11; i++) {
    fprintf(f, "%d or 1 %d %d\n", next_line, opcode_comp + i, next_line - 1);
    next_line++;
  }
  int opcode_test_loc = next_line - 1;
  fprintf(f, "%d bad -%d unknown_opcode\n", next_line,
          next_line - 1); // bad if no recognised opcode is found
  next_line++;

  fprintf(f, ";\n; Bad command\n");
  fprintf(f, "%d or 1 %d %d\n", next_line, command_loc, command_loc + 1);
  next_line++;
  for (int i = 2; i < 49; i++) {
    fprintf(f, "%d or 1 %d %d\n", next_line, command_loc + i, next_line - 1);
    next_line++;
  }
  fprintf(f, "%d and 1 %d -%d\n", next_line, next_line - 1,
          opcode_test_loc); // bad if no recognised command is found
  fprintf(f, "%d bad %d error_in_command(guess_funct3)\n", next_line + 1,
          next_line);

  next_line += 2;
  fprintf(f, ";\n; Bad jump alignment\n");
  fprintf(f, "%d bad %d misaligned_instruction_fetch_ERROR\n", next_line,
          badstate_pretest); // bad if pretest is false
  next_line++;

  return next_line;
}

void relational_btor(FILE *f, state *s, int iterations) {
  int next_line = btor_constants(f);

  int counter_loc =
      next_line + 2; // there are two needed constants before the state
  next_line = btor_counter(f, next_line);

  int reg_const_loc = next_line;
  next_line = btor_register_consts(f, next_line, s);
  int registers = next_line; // PC is assumed as 32th register
  next_line = btor_registers(f, next_line, reg_const_loc, s);

  int reg_init_flag_loc = next_line;
  next_line = btor_register_initialisation_flags(f, next_line, s);

  next_line = btor_memory(f, next_line, s);
  int memory =
      next_line - 2; // last line is initiation, state is the line before

  next_line = btor_get_current_command(f, next_line, registers, memory);
  int command = next_line - 1;

  int codes[7]; //={opcode, rd, rs1, rs2, funct3, funct7};
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
  int immediate = next_line - 1;    // immediate
  int opcode_comp = immediate - 19; // HACKY

  next_line = btor_check_4_all_commands(f, next_line, opcode_comp, codes);
  int command_check_loc = next_line - 49;

  next_line = btor_updates(f, next_line, registers, memory, command_check_loc,
                           immediate, opcode_comp, codes, reg_init_flag_loc);
  int bad_helper_loc = next_line - 1;

  next_line = btor_bad_counter(f, next_line, counter_loc, iterations);

  next_line = btor_bad_command(f, next_line, command_check_loc, opcode_comp,
                               bad_helper_loc);
}

int main(int argc, char *argv[]) {

  char *source;
  char *target = malloc(13 * sizeof(char)); // size of default target file name
  strcpy(target, "output.btor2");
  int iterations = 1;
  bool to_stdout = false;

  FILE *f;

  int opt;

  while ((opt = getopt(argc, argv, "o:n:p")) != -1) {
    switch (opt) {
    case 'o':                                       // output file
      target = realloc(target, strlen(optarg) + 1); // +1 for null terminator
      if (!target) {
        fprintf(stderr, "Memory allocation failed for target file name.\n");
        return 1;
      }
      strcpy(target, optarg);
      break;
    case 'n': // iterations
      iterations = atoi(optarg);
      break;
    case 'p': // print
      to_stdout = true;
      break;
    case '?':
      if (optopt == 'o' || optopt == 'i') {
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      } else {
        fprintf(stderr, "Unknown option `-%c`.\n", optopt);
      }
      return 1;

    default:
      fprintf(stderr,
              "Usage: %s [-o <target>] [-n <iterations>] [-p]"
              "<sourcefile>.state\n",
              argv[0]);
      return 1;
    }
  }
  if (optind >= argc) {
    printf("Expected path to state file after options\n");
    return 1;
  }
  char *source_file_extension = strrchr(argv[optind], '.');
  if (!source_file_extension) {
    printf("No file extension for initial state\n");
    return 1;
  }
  if (strcmp(source_file_extension, ".state")) { // file extension is NOT .state
    printf("Wrong file extension for initial state, expected .state, got %s\n",
           source_file_extension);
    return 1;
  }
  source = argv[optind];

  state *s = create_new_state();

  if (s == NULL) {
    fprintf(stderr, "Failed to create new state.\n");
    return 1;
  }

  if (!load_state(source, s)) {
    fprintf(stderr, "Failed to load state from file: %s\n", argv[1]);
    kill_state(s);
    return 1;
  }

  if (!to_stdout) {
    // Check if target file has .btor2 extension
    char *target_file_extension = strrchr(target, '.');
    if (!target_file_extension) {
      // If no extension, append .btor2
      target = realloc(target, strlen(target) + 7); // 7 for ".btor2"
      if (!target) {
        fprintf(stderr, "Memory allocation failed for target file name.\n");
        kill_state(s);
        return 1;
      }
      strcat(target, ".btor2");
    } else if (strcmp(target_file_extension, ".btor2") != 0) {
      fprintf(stderr, "Output file must have .btor2 extension.\n");
      free(target);
      kill_state(s);
      return 1;
    }
    // Open the target file for writing
    f = fopen(target, "w");
    if (!f) {
      fprintf(stderr, "Failed to open output file: %s\n", target);
      kill_state(s);
      return 1;
    }
  } else {
    f = stdout;
  }
  relational_btor(f, s, iterations);

  kill_state(s);
  fclose(f);
  free(target);
  return 0;
}