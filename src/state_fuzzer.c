#include "./utils/state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Generate a random generated riscv .state file.
 * This file can then be used to test the
 * the simulation via btormc against risc_v_sim.
 * The file contains random pc and register values,
 * as well as a memory with one random command at pc.
 */

#define MEMORY_SIZE 8 // address space size in bits, 8 bits = 256 addresses
#define MEMORY_ADDRESSES                                                       \
  (1 << MEMORY_SIZE) // Maximum memory size, 256 addresses

#define CUT_LOW_BIT_MASK                                                       \
  0b11110 // Mask to cut the lowest bit of a register id (for immediates)

uint64_t rand_uint64_t() {
  uint64_t value = 0;
  // rand gives a random value between 0 and RAND_MAX, which is usually 2^31-1.
  // We generate 4 random halfwords and combine them into a uint64_t.
  for (int i = 0; i < 4; i++) {
    value |= ((uint64_t)rand() & 0xFFFF) << (i * 16);
  }
  return value;
}

int main(int argc, char *argv[]) {

  unsigned int seed = time(NULL); // Default seed value

  bool to_stdout = false;
  char *target_path = malloc(13 * sizeof(char)); // Default target path
  if (!target_path) {
    fprintf(stderr, "Memory allocation failed for default target file name.\n");
    return 1;
  }

  strcpy(target_path, "fuzzed.state");
  int opt;

  while ((opt = getopt(argc, argv, "po:s:")) != -1) {
    switch (opt) {

    case 's': // seed
      seed = atoi(optarg);
      if (seed <= 0) {
        fprintf(stderr, "Seed must be a positive integer.\n");
        return 1;
      }
      break;
    case 'p':
      to_stdout = true;
      break;
    case 'o':
      target_path =
          realloc(target_path, strlen(optarg) + 1); // +1 for null terminator
      if (!target_path) {
        fprintf(stderr, "Memory allocation failed for target file name.\n");
        return 1;
      }
      strcpy(target_path, optarg);
      break;
    case '?':
      fprintf(stderr, "Unknown option: %c\n", optopt);
      return 1;
    default:
      fprintf(stderr, "Usage: %s [-p] [-o <output file>] [-s <seed(int)>]\n",
              argv[0]);
      return 1;
    }
  }

  srand(seed); // Seed the random number generator

  FILE *f;
  if (to_stdout) {
    f = stdout; // Print to stdout
  } else {
    f = fopen(target_path, "w");
    if (!f) {
      fprintf(stderr, "Failed to open output file: %s\n", target_path);
      free(target_path);
      return 1;
    }
  }

  state *s = create_new_state();
  s->pc =
      rand() % MEMORY_ADDRESSES; // Random pc value in the first 256 addresses

  if (s->pc >= MEMORY_ADDRESSES - 5) {
    s->pc = MEMORY_ADDRESSES - 5; // Ensure pc is not out of bounds
  }
  uint64_t pc_mask = ~0x3; // Mask to ensure pc is 4-aligned
  s->pc &= pc_mask;        // Cut the lowest bit of pc

  uint64_t rs1 = rand() % 32; // Random rs1 register
  uint64_t rs2 = rand() % 32; // Random rs2 register
  uint64_t rd = rand() % 32;  // Random rd register
  for (size_t i = 1; i < 32;
       i++) { // Register x0 is always initialised and zero
    if (rand() % 2 == 0 && i != rs1 &&
        i != rs2) // Randomly decide if register is initialised
    {
      s->regs_init[i] = false; // Register is not initialised
    } else {
      s->regs_values[i] = rand_uint64_t(); // Random register values
      s->regs_init[i] = true;              // All registers are initialised
    }
  }

  uint32_t rd_placed = rd << 7;    // Shift rd to the correct position
  uint32_t rs1_placed = rs1 << 15; // Shift rs1 to the correct position
  uint32_t rs2_placed = rs2 << 20; // Shift rs2 to the correct position

  uint32_t sixth_bit_for_shift =
      ((rand() % 2) << 25); // Random 6. bit for shift commands, where lowest
                            // bit of funct7 is part of shift amount

  uint32_t funct7imm =
      (rand() % 128) << 25; // Random legal funct7 value for immediate commands,
                            // where lowest bit is part of immediate value
  uint32_t funct3imm = (rand() % 8) << 12;

  // Create a random command
  uint32_t command_base[] = {
      0x00000037, // lui x0 0
      0x00000017, // auipc x0 0
      0x0000006F, // jal x0 0
      0x00000067, // jalr x0 x0 0
      0x00000063, // beq x0 x0 0
      0x00001063, // bne x0 x0 0
      0x00004063, // blt x0 x0 0
      0x00005063, // bge x0 x0 0
      0x00006063, // bltu x0 x0 0
      0x00007063, // bgeu x0 x0 0
      0x00000003, // lb x0 x0 0
      0x00001003, // lh x0 x0 0
      0x00002003, // lw x0 x0 0
      0x00003003, // ld x0 x0 0
      0x00004003, // lbu x0 x0 0
      0x00005003, // lhu x0 x0 0
      0x00006003, // lwu x0 x0 0
      0x00000023, // sb x0 0 x0
      0x00001023, // sh x0 0 x0
      0x00002023, // sw x0 0 x0
      0x00003023, // sd x0 0 x0
      0x00000013, // addi x0 x0 0
      0x00002013, // slti x0 x0 0
      0x00003013, // sltiu x0 x0 0
      0x00004013, // xori x0 x0 0
      0x00006013, // ori x0 x0 0
      0x00007013, // andi x0 x0 0
      0x00001013, // slli x0 x0 0
      0x00005013, // srli x0 x0 0
      0x40002013, // srai x0 x0 0
      0x00000033, // add x0 x0 x0
      0x40000033, // sub x0 x0 x0
      0x00001033, // sll x0 x0 x0
      0x00002033, // slt x0 x0 x0
      0x00003033, // sltu x0 x0 x0
      0x00004033, // xor x0 x0 x0
      0x00005033, // srl x0 x0 x0
      0x40005033, // sra x0 x0 x0
      0x00006033, // or x0 x0 x0
      0x00007033, // and x0 x0 x0
      0x0000001b, // addiw x0 x0 0
      0x0000101b, // slliw x0 x0 0
      0x0000501b, // srliw x0 x0 0
      0x4000501b, // sraiw x0 x0 0
      0x0000003b, // addw x0 x0 0
      0x4000003b, // subw x0 x0 0
      0x0000103b, // sllw x0 x0 0
      0x0000503b, // srlw x0 x0 0
      0x4000503b  // sraw x0 x0 0
  };
  int command_picker =
      rand() % (sizeof(command_base) /
                sizeof(command_base[0])); // Randomly pick a command
  bool pc_influence = false;
  bool load = false;
  bool store = false;

  bool r_type = false;     // R-type commands
  bool i_type = false;     // I-type commands
  bool s_type = false;     // S-type commands
  bool b_type = false;     // B-type commands
  bool u_type = false;     // U-type commands
  bool j_type = false;     // J-type commands
  bool shift_type = false; // Shift commands with 6bit shift ammount

  uint32_t command = command_base[command_picker];

  if (command_picker < 10 && command_picker >= 2) {
    pc_influence = true; // Branch and jump commands
  }
  if (command_picker >= 10 && command_picker < 17) {
    load = true; // Load commands
  }
  if (command_picker >= 17 && command_picker < 21) { // Store commands
    store = true;
  }

  if (command_picker < 2) {
    u_type = true; // lui and auipc
  } else if (command_picker == 2) {
    j_type = true;  // jal
    rs1_placed = 0; // bits 15-19 are too high for address space of 8bit
    rs2_placed &= CUT_LOW_BIT_MASK
                  << 21; // bit 11 is too high for address space of 8bit, also
                         // must be 4-aligned
    funct7imm &= 0b111 << 25; // bits 31-25 are too high for address space of
                              // 8bit, also must be 4-aligned
    if (s->pc + (rs2_placed >> 20) + (funct7imm >> 20) >= MEMORY_ADDRESSES) {
      s->pc = (rand() % (MEMORY_ADDRESSES - (rs2_placed >> 20) -
                         (funct7imm >> 20)) >>
               2)
              << 2;
    }
  } else if (command_picker == 3) {
    i_type = true; // jalr
    if (rs2 + get_register(s, rs1) >= MEMORY_ADDRESSES && rs1 != 0) {
      set_register(s, rs1, rand() % 128); // Ensure rs2 value is low enough that
                                          // it does not overflow the memory
    }
    int alignment_bad = rs2 + get_register(s, rs1) % 4;
    if (alignment_bad && rs1 != 0) {
      set_register(s, rs1,
                   get_register(s, rs1) - alignment_bad); // Ensure address is
                                                          // 4-aligned
    }

  } else if (command_picker >= 4 && command_picker < 10) {
    b_type = true; // beq, bne, blt, bge, bltu, bgeu
    rd_placed &= CUT_LOW_BIT_MASK
                 << 7; // rd is not used in branch commands, so we can cut it
    if ((rd + s->pc) >= MEMORY_ADDRESSES) {
      rd_placed = rand() % (MEMORY_ADDRESSES - s->pc -
                            1);      // Ensure rd value is low enough that
                                     // it does not overflow the memory
      rd_placed &= CUT_LOW_BIT_MASK; // this can only lower rd
      rd = rd_placed;                // Set rd to the new value
      rd_placed = rd_placed << 7;    // Shift rd to the correct position
    }
    if (rd + s->pc < MEMORY_ADDRESSES - 64) {
      command |=
          sixth_bit_for_shift; // Immediate bit 5 can be set if
                               // rd + pc is low enough, otherwise it must be 0
    }

  } else if (command_picker >= 10 && command_picker < 17) {
    i_type = true; // lb, lh, lw, ld, lbu, lhu, lwu
    if (rs1 != 0) {
      set_register(s, rs1,
                   rand() % MEMORY_ADDRESSES -
                       9); // Ensure rs1 value is low enough that it does not
                           // overflow the memory
    }
    if (get_register(s, rs1) + rs2 >= MEMORY_ADDRESSES - 9) {
      rs2_placed =
          0; // Ensure rs2 value is low enough that it does not overflow the
             // memory
             // could be prettier, but this should be enough
      rs2 = 0;
    }
    set_doubleword(
        s, get_register(s, rs1) + rs2,
        rand_uint64_t()); // Set a random value in memory at the address

  } else if (command_picker >= 17 && command_picker < 21) {
    s_type = true; // sb, sh, sw, sd
    if (rs1 != 0) {
      set_register(s, rs1,
                   rand() % MEMORY_ADDRESSES -
                       9); // Ensure rs2 value is low enough that it does not
                           // overflow the memory
    }
    if (get_register(s, rs1) + rd >= MEMORY_ADDRESSES - 9) {
      rd = rand() % (MEMORY_ADDRESSES - 9 - get_register(s, rs1));
      rd_placed = rd << 7;
    }

    if (rd + get_register(s, rs1) < MEMORY_ADDRESSES - 64) {
      command |=
          sixth_bit_for_shift; // Immediate bit 5 can be set if
                               // rd + pc is low enough, otherwise it must be 0
    }
  } else if (command_picker >= 21 && command_picker < 27) {
    i_type = true; // addi, slti, sltiu, xori, ori, andi
  } else if (command_picker >= 27 && command_picker < 30) {
    shift_type = true; // slli, srli, srai
  } else if (command_picker >= 30 && command_picker < 40) {
    r_type = true; // add, sub, sll, slt, sltu, xor, srl, sra, or, and
  } else if (command_picker == 40) {
    i_type = true; // addiw
  } else if (command_picker >= 41 && command_picker < 49) {
    r_type = true; // slliw, srliw, sraiw, addw, subw, sllw, srlw, sraw
  }

  command |= rd_placed | rs1_placed | rs2_placed;
  if (u_type /*|| j_type*/) { // immediate gets too high with func3 immediate
    command |= funct3imm;
  }
  if (!r_type && !pc_influence && !load && !store &&
      !shift_type) // avoid too high immediates with jumps
  {
    command |= funct7imm;
  }
  if (shift_type) {
    command |= sixth_bit_for_shift; // Set the 6th bit for shift commands
  }

  set_word(s, s->pc, command); // Set the command at the pc address

  echo_and_kill_state_keep_seed(s, f,
                                &seed); // Print the state to the file or stdout
  if (f != stdout) {
    fclose(f); // Close the file if it was opened
  }
  free(target_path); // Free the target path memory

  return 0;
}