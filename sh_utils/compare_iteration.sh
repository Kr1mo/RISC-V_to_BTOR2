#!/bin/bash -e

# Beware of possible path issues. Call this script from its parent directory.
# Ensure correct path to btormc

# This script takes a *.state file as an argument, converts it to BTOR format,
# runs it through btormc, and then processes the output with restate.

# Check if a *.state file is provided as an argument
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 [-r] [-k] <file.state>"
    exit 1
fi

# Parse options
REPEAT_MODE=false
KEEP_STATE_FILES=false
KEEP_WITNESS_FILES=false
while getopts ":rkw" opt; do
    case $opt in
        r)
            REPEAT_MODE=true
            ;;
        k)
            KEEP_STATE_FILES=true
            ;;
        w)
            KEEP_WITNESS_FILES=true
            ;;
        *)
            echo "Invalid option: -$OPTARG" >&2
            exit 1
            ;;
    esac
done
shift $((OPTIND - 1))

# Ensure the "differences" folder exists if in repeat mode
if $REPEAT_MODE; then
    mkdir -p sh_utils/diffs
fi

# Ensure the "generated_states" folder exists
if $KEEP_STATE_FILES; then
    mkdir -p sh_utils/generated_states
fi
# Ensure the "witness" folder exists if keeping witness files
if $KEEP_WITNESS_FILES; then
    mkdir -p sh_utils/witness
fi

# Get the state file and its base name
STATE_FILE="$1"
BASE_NAME=$(basename "$STATE_FILE" .state)

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

# Check if the tools exist
if [[ ! -x "$RISCV_TO_BTOR" || ! -x "$BTORMC" || ! -x "$RESTATE" || ! -x "$RISCVSIM" ]]; then
    echo "Error: One or more required tools are not found or not executable."
    exit 1
fi

# Additional arguments
RISCV_TO_BTOR_ARGS="-p"
BTORMC_ARGS="--trace-gen-full"
RESTATE_ARGS="-o sh_utils/${BASE_NAME}_btor2.state"
RISCVSIM_ARGS="-e sh_utils/${BASE_NAME}_sim.state"

# Run BTOR2 sim
"$RISCV_TO_BTOR" $RISCV_TO_BTOR_ARGS "$STATE_FILE" | "$BTORMC" $BTORMC_ARGS > sh_utils/${BASE_NAME}_witness.tmp
"$RESTATE" $RESTATE_ARGS sh_utils/${BASE_NAME}_witness.tmp

if $KEEP_WITNESS_FILES; then
    mv sh_utils/${BASE_NAME}_witness.tmp sh_utils/witness/
else
    rm sh_utils/${BASE_NAME}_witness.tmp
fi

# Run my sim
"$RISCVSIM" $RISCVSIM_ARGS "$STATE_FILE"

# Compare the two state files
diff --unified=2 sh_utils/${BASE_NAME}_btor2.state sh_utils/${BASE_NAME}_sim.state > sh_utils/${BASE_NAME}.diff || true

if $REPEAT_MODE; then
    # If in repeat mode, move the diff file to the differences folder
    if [ -s sh_utils/${BASE_NAME}.diff ]; then
        mv sh_utils/${BASE_NAME}.diff sh_utils/diffs/
    else
        rm sh_utils/${BASE_NAME}.diff
    fi
else
    # If not in repeat mode, just print the diff
    if [ -s sh_utils/${BASE_NAME}.diff ]; then
        echo "Differences found:"
        cat sh_utils/${BASE_NAME}.diff
    else
        echo "No differences found."
    fi 
fi

# Clean up
if ! $KEEP_STATE_FILES; then
    rm -f sh_utils/${BASE_NAME}_btor2.state sh_utils/${BASE_NAME}_sim.state
fi
rm -f sh_utils/${BASE_NAME}.diff
