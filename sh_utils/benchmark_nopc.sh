#!/bin/bash

# Check if an identifier is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <identifier>"
    echo "Run from main directory!"
    exit 1
fi

identifier=$1

# Directory containing benchmark files
BENCHMARK_DIR="benchmark_files/nopc"
TEMP_DIR="benchmark_files/temp_files"
# Log file to store timing results
LOG_FILE="benchmark_files/bench_nopc_${identifier}.log"

# btormc executable path
BTORMC_EXECUTABLE="/home/moell/Documents/Bachelor-Thesis/boolector/build/bin/btormc"

# Clear the log file if it exists
> "$LOG_FILE"
if [[ ! -d "$TEMP_DIR" ]]; then
    mkdir -p "$TEMP_DIR"
fi

# Iterate over all files in the benchmark directory
for file in "$BENCHMARK_DIR"/*.btor2; do
    if [[ -f "$file" ]]; then
        echo "Processing $file..."
    BASE_NAME=$(basename "$file" .btor2)
        
        
        # Append the benchmark name to the log file
        echo -n "Benchmark: $BASE_NAME" >> "$LOG_FILE"
        # Run btormc and measure the time
        { time "$BTORMC_EXECUTABLE" -kmax 10000 --trace-gen 0 "$file"; } 2>> "$LOG_FILE"
        echo "" >> "$LOG_FILE"
    fi
done

echo "Benchmarking nopc completed. Results saved in $LOG_FILE."