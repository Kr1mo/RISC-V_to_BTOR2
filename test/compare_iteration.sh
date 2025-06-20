#!/bin/bash -e

# Beware of possible path issues. Call this script from its parent directory.
# Ensure correct path to btormc

# This script takes a *.state file as an argument, converts it to BTOR format,
# runs it through btormc, and then processes the output with restate.

# Check if a *.state file is provided as an argument
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <file.state>"
    exit 1
fi

STATE_FILE="$1"

# Check if the file exists and has the correct extension
if [[ ! -f "$STATE_FILE" || "${STATE_FILE##*.}" != "state" ]]; then
    echo "Error: File does not exist or is not a *.state file."
    exit 1
fi

# Define the tool paths
RISCV_TO_BTOR="bin/riscv_to_btor2"
BTORMC="/home/moell/Documents/Bachelor-Thesis/boolector/build/bin/btormc"
RESTATE="bin/restate_witness"
RISCVSIM="/home/moell/Documents/Risc-V-Simulator/risc_v_sim"

# Additional arguments
RISCV_TO_BTOR_ARGS="-p"
BTORMC_ARGS="--trace-gen-full"
RESTATE_ARGS="-o little_scripts/btor2.state"
RISCVSIM_ARGS="-e little_scripts/sim.state"

# Run BTOR2 sim
"$RISCV_TO_BTOR" $RISCV_TO_BTOR_ARGS "$STATE_FILE" | "$BTORMC" $BTORMC_ARGS > little_scripts/witness.tmp
"$RESTATE" $RESTATE_ARGS little_scripts/witness.tmp
rm little_scripts/witness.tmp

# Run my sim
"$RISCVSIM" $RISCVSIM_ARGS "$STATE_FILE"

# Compare the two state files
diff --unified=2 little_scripts/btor2.state little_scripts/sim.state > little_scripts/compare_iteration.diff || true
if [ -s little_scripts/compare_iteration.diff ]; then
    echo "Differences found:"
    cat little_scripts/compare_iteration.diff
else
    echo "No differences found."
fi

# Clean up
rm little_scripts/btor2.state little_scripts/sim.state
rm -f little_scripts/compare_iteration.diff