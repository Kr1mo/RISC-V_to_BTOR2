#!/bin/bash

# Check if the user provided the number of tests as an argument
if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <number_of_tests> <Identifier>"
    exit 1
fi

# Check if the user wants to keep the initial states
del_bad_states=false
if [ "${3:-}" == "--del-all-states" ]; then
    del_bad_states=true
fi
keep_all_states=false
if [ "${3:-}" == "--keep-all-states" ]; then
    keep_all_states=true
fi

# Get the number of tests from the argument
num_tests=$1

identifier=$2

# Check if the argument is a valid positive integer
if ! [[ "$num_tests" =~ ^[0-9]+$ ]]; then
    echo "Error: Argument must be a positive integer."
    exit 1
fi
# Check if the identifier is provided and not empty
if [ -z "$identifier" ]; then
    echo "Error: Identifier cannot be empty."
    exit 1
fi
# Check if the identifier is a valid string (alphanumeric and underscores)
if ! [[ "$identifier" =~ ^[a-zA-Z0-9_]+$ ]]; then
    echo "Error: Identifier must be alphanumeric and can include underscores."
    exit 1
fi

# Ensure the necessary directories exist
mkdir -p sh_utils/diffs
mkdir -p sh_utils/generated_states

# Check if the statefuzzer binary exists
if [ ! -f ./bin/state_fuzzer ]; then
    echo "Error: state_fuzzer binary not found in ./bin directory."
    exit 1
fi
# Check if the compare_iteration script exists
if [ ! -f ./sh_utils/compare_iteration.sh ]; then
    echo "Error: compare_iteration.sh script not found in sh_utils directory."
    exit 1
fi
# Ensure the compare_iteration script is executable
chmod +x ./sh_utils/compare_iteration.sh

# Loop to run the specified number of tests
for ((i=1; i<=num_tests; i++)); do

    # get a random seed for the statefuzzer
    seed=$((RANDOM * RANDOM))

    echo "Running test $i with seed $seed ..."

    # Generate a random state using statefuzzer
    ./bin/state_fuzzer -o "sh_utils/generated_states/${identifier}_$i.state" -s $seed


    ./sh_utils/compare_iteration.sh -r "sh_utils/generated_states/${identifier}_$i.state"


    # Check if compare_iterations succeeded
    if [ $? -ne 0 ]; then
        echo "Error: compare_iterations failed for test $i."
        exit 1
    fi

    if [ -f "sh_utils/diffs/${identifier}_$i.diff" ]; then
        echo "Differences found for test $i. Check sh_utils/diffs/${identifier}_$i.diff for details."

        # remove the generated state file if not keeping it
        if $del_bad_states; then
            rm "sh_utils/generated_states/${identifier}_$i.state"
        fi
    else
        echo "No differences found for test $i."
        # remove the generated state file
        if ! $keep_all_states; then
        rm "sh_utils/generated_states/${identifier}_$i.state"
        fi
    fi


done

# log the completion of all tests if there are states in generated_states
if [ "$(ls -A sh_utils/generated_states)" ]; then
    echo "Number of failed tests: $(ls sh_utils/generated_states | grep "^${identifier}_" | wc -l)"
    sh_utils/diff_logger.sh "$identifier"
else
    echo "No test failed."
fi

echo "All tests completed."