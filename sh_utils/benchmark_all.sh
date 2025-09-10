#!/bin/bash

# Check if an identifier is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <identifier>"
    echo "Run from main directory!"
    exit 1
fi

identifier=$1

sh_utils/benchmark_base.sh "$identifier"
sh_utils/benchmark_fullmem.sh "$identifier"
sh_utils/benchmark_nopc.sh "$identifier"
sh_utils/benchmark_extendedaddr.sh "$identifier"