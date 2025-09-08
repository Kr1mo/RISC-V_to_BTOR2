#!/bin/bash

sh_utils/benchmark_all.sh bigbench01
sh_utils/benchmark_all.sh bigbench02
sh_utils/benchmark_all.sh bigbench03
sh_utils/benchmark_all.sh bigbench04
sh_utils/benchmark_all.sh bigbench05

for file in benchmark_files/bench*.log; do
    TYPE=$(basename "$file" | cut -d'_' -f2 | cut -d'.' -f1)
    NUM=$(basename "$file" | cut -d'_' -f3 | cut -d'.' -f1 | sed 's/bigbench//')
    cat $file | grep 'real' >> benchmark_files/summary_"$TYPE".log
done