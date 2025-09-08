#!/bin/bash

sh_utils/benchmark_all.sh bigbench01
sh_utils/benchmark_all.sh bigbench02
sh_utils/benchmark_all.sh bigbench03
sh_utils/benchmark_all.sh bigbench04
sh_utils/benchmark_all.sh bigbench05
sh_utils/benchmark_all.sh bigbench06
sh_utils/benchmark_all.sh bigbench07
sh_utils/benchmark_all.sh bigbench08
sh_utils/benchmark_all.sh bigbench09
sh_utils/benchmark_all.sh bigbench10
sh_utils/benchmark_all.sh bigbench11
sh_utils/benchmark_all.sh bigbench12
sh_utils/benchmark_all.sh bigbench13
sh_utils/benchmark_all.sh bigbench14
sh_utils/benchmark_all.sh bigbench15

for file in benchmark_files/bench*.log; do
    TYPE=$(basename "$file" | cut -d'_' -f2 | cut -d'.' -f1)
    NUM=$(basename "$file" | cut -d'_' -f3 | cut -d'.' -f1 | sed 's/bigbench//')
    cat $file | grep 'real' >> benchmark_files/summary_"$TYPE".log
done