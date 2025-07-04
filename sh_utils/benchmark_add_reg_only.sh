#!/bin/bash

# Directory containing benchmark files
BENCHMARK_DIR="benchmark_files"
TEMP_DIR="benchmark_files/temp_files"
# Log file to store timing results
LOG_FILE="bench_add.log"

# btormc executable path
BTORMC_EXECUTABLE="/home/moell/Documents/Bachelor-Thesis/boolector/build/bin/btormc"

# Clear the log file if it exists
> "$LOG_FILE"
if [[ ! -d "$TEMP_DIR" ]]; then
    mkdir -p "$TEMP_DIR"
fi

# Iterate over all files starting with "add_" in the benchmark directory
for file in "$BENCHMARK_DIR"/*.state; do
    if [[ -f "$file" ]]; then
        echo "Processing $file..."
    BASE_NAME=$(basename "$file" .state)
        
        # Generate BTOR2 file using riscv_to_btor2
        btor2_file="$TEMP_DIR/$BASE_NAME.btor2"
        ./bin/riscv_to_btor2 -p -n -1 "$file" > "$btor2_file"
        
        # Run btormc and measure the time
        { time "$BTORMC_EXECUTABLE" -kmax 5000 "$btor2_file"; } 2>> "$LOG_FILE"
        
        echo "Finished processing $file" >> "$LOG_FILE"
    fi
done

echo "Benchmarking completed. Results saved in $LOG_FILE."