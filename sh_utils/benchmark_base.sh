#!/bin/bash

# Check if an identifier is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <identifier>\nRun from base directory!"
    exit 1
fi

identifier=$1

# Directory containing benchmark files
BENCHMARK_DIR="benchmark_files/base"
TEMP_DIR="benchmark_files/temp_files"
# Log file to store timing results
LOG_FILE="benchmark_files/bench_base_${identifier}.log"

# btormc executable path
BTORMC_EXECUTABLE="/home/moell/Documents/Bachelor-Thesis/boolector/build/bin/btormc"

# Clear the log file if it exists
> "$LOG_FILE"
if [[ ! -d "$TEMP_DIR" ]]; then
    mkdir -p "$TEMP_DIR"
fi

# Iterate over all files in the benchmark directory
for file in "$BENCHMARK_DIR"/*.state; do
    if [[ -f "$file" ]]; then
        echo "Processing $file..."
    BASE_NAME=$(basename "$file" .state)
        
        # Generate BTOR2 file using riscv_to_btor2
        btor2_file="$TEMP_DIR/$BASE_NAME.btor2"
        ./bin/riscv_to_btor2 -p -n -1 "$file" > "$btor2_file"
        
        # Append the benchmark name to the log file
        echo -n "Benchmark: $BASE_NAME" >> "$LOG_FILE"
        # Run btormc and measure the time
        { time "$BTORMC_EXECUTABLE" -kmax 10000 --trace-gen 0 "$btor2_file"; } 2>> "$LOG_FILE"
        echo "" >> "$LOG_FILE"

        # Clean up temporary BTOR2 file
#        rm "$btor2_file"
    fi
done

echo "Benchmarking base completed. Results saved in $LOG_FILE."