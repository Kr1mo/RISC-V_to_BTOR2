// Needs btormc --trace-gen-full to function properly
#include "./utils/state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void close_if_not_std(FILE *file) {
  if (file != stdin && file != stdout) {
    fclose(file);
  }
}

int main(int argc, char *argv[]) {
  bool from_stdin = false;
  bool to_stdout = false;
  char *target_path =
      malloc(13 * sizeof(char)); // size of default target file name
  target_path = strcpy(target_path, "output.state");
  FILE *target_file;
  FILE *witness_file;

  int opt;

  while ((opt = getopt(argc, argv, "ipo:")) != -1) {
    switch (opt) {
    case 'i': // immediate
      from_stdin = true;
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
      fprintf(stderr,
              "Usage: %s [-i] [-p] [-o <output file>] [<witness file>]\n",
              argv[0]);
      return 1;
    }
  }

  if (optind >= argc && from_stdin) {
    printf("No state file provided, reading from stdin\n");
    witness_file = stdin;
  } else if (optind < argc) {
    witness_file = fopen(argv[optind], "r");
    if (!witness_file) {
      fprintf(stderr, "Error opening witness file: %s\n", argv[optind]);
      return 1;
    }
  } else {
    fprintf(stderr, "No state file provided.\n");
    free(target_path);
    return 1;
  }
  if (to_stdout) {
    target_file = stdout;
  } else {
    // check for file extension
    char *file_extension = strrchr(target_path, '.');
    // If no extension, add .state
    if (!file_extension) {
      target_path = realloc(target_path, strlen(target_path) + 7);
      if (!target_path) {
        fprintf(stderr, "Memory allocation failed\n");
        close_if_not_std(witness_file);
        free(target_path);
        return 1;
      }
      strcat(target_path, ".state");
    }
    if (file_extension && strcmp(file_extension, ".state") != 0) {
      fprintf(stderr, "Target file must have .state extension\n");
      close_if_not_std(witness_file);
      free(target_path);
      return 1;
    }
    target_file = fopen(target_path, "w");
    if (!target_file) {
      fprintf(stderr, "Error opening target file: %s\n", target_path);
      close_if_not_std(witness_file);
      free(target_path);
      return 1;
    }
  }

  char buff[256]; // Buffer for reading lines. Size is plenty for this use case.
                  // 74 are needed at mosst for up to 99 iterations
  char *buff_ptr = buff; // Pointer to the buffer

  long int last_state_part = -1;

  while (fgets(buff, sizeof(buff), witness_file) != NULL) {
    if (buff[0] != '#') {
      continue; // Only find starts of state_part
    } else {
      // Save the position of the last state part
      last_state_part = ftell(witness_file);
    }
  }
  if (last_state_part == -1) {
    fprintf(stderr, "No state part found in witness file.\n");
    close_if_not_std(witness_file);
    close_if_not_std(target_file);
    return 1;
  }
  // Reset the file pointer to the last state part
  fseek(witness_file, last_state_part, SEEK_SET);
  buff_ptr = fgets(buff, sizeof(buff), witness_file); // Skip iterations counter
  if (buff_ptr && (strncmp(buff, "0 ", 2) ||
                   strncmp(buff + 66, " iterations_counter#", 20))) {
    fprintf(stderr,
            "Invalid state part format. Expected iterations counter, got:\n%s",
            buff);
    close_if_not_std(witness_file);
    close_if_not_std(target_file);
    return 1;
  }

  // Read the state part
  state *s = create_new_state();
  char *token;
  for (int i = 1; i <= 32; i++) {
    buff_ptr = fgets(buff, sizeof(buff), witness_file);
    token = strtok(buff, " ");
    char id_str[8];           // Buffer for ID string
    sprintf(id_str, "%d", i); // Convert i to string
    if (token == NULL || strcmp(token, id_str) != 0) {
      fprintf(stderr,
              "Invalid state part format. Expected ID %d for assignment of "
              "x%d, got: %s\n",
              i, i - 1, buff);
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }
    token = strtok(NULL, " ");
    if (token == NULL || strlen(token) != 64) { // 64bit registers
      fprintf(stderr,
              "Invalid state part format. Expected valid 64bit binary register "
              "value "
              "for x%d, got: %s\nwith length of %d\n",
              i - 1, token, token ? (int)strlen(token) : 0);
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }
    set_register(s, i - 1,
                 strtoull(token, NULL, 2)); // Convert binary string to long int
  }
  buff_ptr = fgets(buff, sizeof(buff), witness_file); // This should be pc
  token = strtok(buff, " ");
  token = strtok(NULL, " "); // Skip the ID part
  char *pc_value_str = token;
  token = strtok(NULL, " ");
  if (token == NULL || strncmp(token, "pc", 2) != 0) {
    fprintf(stderr,
            "Invalid state part format. Expected pc assignment, got:\n%s",
            buff);
    close_if_not_std(witness_file);
    close_if_not_std(target_file);
    return 1;
  }
  set_pc(s, strtol(pc_value_str, NULL, 2)); // Convert binary string to long int

  // Read the initialisation flags
  for (int i = 0; i < 32; i++) {
    buff_ptr = fgets(buff, sizeof(buff), witness_file);
    token = strtok(buff, " "); // ID
    token = strtok(NULL, " ");
    if (token == NULL || strlen(token) != 1) { // flags
      fprintf(stderr,
              "Invalid state part format. Expected flag "
              "for x%d, got: %s\n",
              i, token);
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }
    s->regs_init[i] = (token[0] == '1'); // Convert to boolean
  }

  // Read memory values
  while (fgets(buff, sizeof(buff), witness_file) != NULL && buff[0] != '@') {
    if (buff[0] == '\n') {
      fprintf(stderr, "Unexpected empty line in memory section.\n");
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }
    token = strtok(buff, " ");
    if (token == NULL) {
      fprintf(stderr, "Invalid memory line format: %s", buff);
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }
    token = strtok(NULL, " ");
    if (token == NULL || token[0] != '[' || token[strlen(token) - 1] != ']') {
      fprintf(
          stderr,
          "Invalid memory line format, address format expects '[]', got: %s\n",
          token);
      fprintf(stderr, "In following line: %s", buff);
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }
    token++;                                   // Skip the '['
    long int address = strtol(token, NULL, 2); // Convert hex string to long int

    token = strtok(NULL, " ");
    if (token == NULL || strlen(token) != 8) { // byte memory values
      fprintf(stderr, "Invalid memory value format for address %s: %s", token,
              buff);
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }

    long int value =
        strtol(token, NULL, 2); // Convert binary string to long int

    token = strtok(NULL, " ");
    if (token == NULL || strncmp(token, "memory", 6) != 0) {
      fprintf(stderr,
              "Invalid memory line format. Expected 'memory' name, got: %s at "
              "address %lx (hex)\n",
              token, address);
      close_if_not_std(witness_file);
      close_if_not_std(target_file);
      return 1;
    }

    set_byte(s, address, value);
  }

  echo_and_kill_state_keep_seed(
      s, target_file,
      NULL); // Write the state to the target file. No seed to be kept

  // Close files if they are not stdin/stdout
  close_if_not_std(witness_file);
  close_if_not_std(target_file);

  free(target_path); // path no longer needed

  return 0;
}