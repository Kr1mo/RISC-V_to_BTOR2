#!/bin/bash

# Check if an identifier is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <identifier>"
    exit 1
fi

identifier=$1
log_file="sh_utils/${identifier}.log"
states_dir="sh_utils/generated_states"
diffs_dir="sh_utils/diffs"

# Ensure directories exist
if [ ! -d "$states_dir" ] || [ ! -d "$diffs_dir" ]; then
    echo "Error: Directories 'generated_states' or 'diffs' not found."
    exit 1
fi

# Create or clear the log file
echo "--- \*_btor2.state" > "$log_file"
echo "+++ \*_sim.state" >> "$log_file"
echo "" >> "$log_file"

# Process each .state file
for state_file in "$states_dir"/${identifier}_*.state; do
    # Extract the seed (second line of the file)
    seed=$(sed -n '2p' "$state_file" | grep -o '[0-9]\+')

    # Extract the PC value from line 3
    pc_value=$(sed -n '3p' "$state_file" | awk -F':' '{print $2}' | sed 's/#.*//;s/^[ \t]*//;s/[ \t]*$//')

    # Find the line starting with the PC value after "MEMORY:"
    current_command=$(awk -v pc="$pc_value" '/MEMORY:/ {found=1; next} found && $0 ~ "^"pc":" {print $0; exit}' "$state_file" | awk -F':' '{print $2}' | sed 's/#.*//;s/^[ \t]*//;s/[ \t]*$//' | tr -d ' ')

    # Convert the current command from hex to binary
    current_command_binary=$(echo "obase=2; ibase=16; ${current_command^^}" | bc)

    # Ensure current_command_binary has a length of 32
    binary_length=${#current_command_binary}
    if [ "$binary_length" -gt 32 ]; then
        echo "Error: Binary command length exceeds 32 bits."
        exit 1
    elif [ "$binary_length" -lt 32 ]; then
        current_command_binary=$(echo "$current_command_binary" | sed -e :a -e 's/^.\{1,31\}$/0&/;ta')
    fi

    # Find the corresponding .diff file
    diff_file="$diffs_dir/$(basename "$state_file" .state).diff"

    if [ -f "$diff_file" ]; then
        diff_lines=$(grep -E '^\+|^-' "$diff_file" | grep -Ev '^\+\+\+|^---')
    else
        diff_lines="No corresponding diff file found."
    fi

    # Determine the command based on opcode, funct3, and funct7
    opcode=$(echo "$current_command_binary" | cut -c26-32)
    funct3=$(echo "$current_command_binary" | cut -c18-20)
    funct7=$(echo "$current_command_binary" | cut -c1-7)
    command_name=""

    case "$opcode" in
        "0110011") # R-type instructions
            case "$funct3" in
                "000")
                    if [ "$funct7" == "0000000" ]; then
                        command_name="ADD"
                    elif [ "$funct7" == "0100000" ]; then
                        command_name="SUB"
                    fi
                    ;;
                "111") command_name="AND" ;;
                "110") command_name="OR" ;;
                "100") command_name="XOR" ;;
                "001") command_name="SLL" ;;
                "101")
                    if [ "$funct7" == "0000000" ]; then
                        command_name="SRL"
                    elif [ "$funct7" == "0100000" ]; then
                        command_name="SRA"
                    fi
                    ;;
                "010") command_name="SLT" ;;  # Set Less Than
                "011") command_name="SLTU" ;; # Set Less Than Unsigned
                *) command_name="Unknown R-type" ;;
            esac
            ;;
        "0010011") # I-type instructions 
            case "$funct3" in
                "000") command_name="ADDI" ;;
                "111") command_name="ANDI" ;;
                "110") command_name="ORI" ;;
                "100") command_name="XORI" ;;
                "001") command_name="SLLI" ;;
                "101")
                    if [ "$funct7" == "0000000" ]; then
                        command_name="SRLI"
                    elif [ "$funct7" == "0100000" ]; then
                        command_name="SRAI"
                    fi
                    ;;
                *) command_name="Unknown I-type" ;;
            esac
            ;;
        "0111011") # W-type instructions
            case "$funct3" in
                "000")
                    if [ "$funct7" == "0000000" ]; then
                    command_name="ADDW"
                    elif [ "$funct7" == "0100000" ]; then
                    command_name="SUBW"
                    else
                    command_name="Unknown W-type ADD/SUB"
                    fi
                    ;;
                "001") command_name="SLLW" ;;
                "101")
                    if [ "$funct7" == "0000000" ]; then
                    command_name="SRLW"
                    elif [ "$funct7" == "0100000" ]; then
                    command_name="SRAW"
                    fi
                    ;;
                *) command_name="Unknown W-type" ;;
            esac
            ;;
        "0011011") # IW-type instructions
            case "$funct3" in
            "000") 
                command_name="ADDIW"
                ;;
            "001") command_name="SLLIW" ;;
            "101")
                if [ "$funct7" == "0000000" ]; then
                command_name="SRLIW"
                elif [ "$funct7" == "0100000" ]; then
                command_name="SRAIW"
                fi
                ;;
            *) command_name="Unknown IW-type" ;;
            esac
            ;;
        "0000011") # Load instructions
            case "$funct3" in
            "000") command_name="LB" ;;  # Load Byte
            "001") command_name="LH" ;;  # Load Halfword
            "010") command_name="LW" ;;  # Load Word
            "011") command_name="LD" ;;  # Load Doubleword
            "100") command_name="LBU" ;; # Load Byte Unsigned
            "101") command_name="LHU" ;; # Load Halfword Unsigned
            "110") command_name="LWU" ;; # Load Word Unsigned
            *) command_name="Unknown Load-type" ;;
            esac
            ;;
        "0100011") command_name="STORE" ;; # Store instructions
        "1100011") command_name="BRANCH" ;; # Branch instructions
        "1101111") command_name="JAL" ;; # Jump and Link
        "1100111") command_name="JALR" ;; # Jump and Link Register
        "0110111") command_name="LUI" ;; # Load Upper Immediate
        "0010111") command_name="AUIPC" ;; # Add Upper Immediate to PC
        *) command_name="Unknown Opcode" ;;
    esac

    # Extract the rd register if applicable
    rd=""
    case "$opcode" in
        "0110011"|"0010011"|"0000011"|"1101111"|"1100111"|"0110111"|"0010111"|"0011011"|"0111011") # Instructions that use rd
            rd=$(echo "$current_command_binary" | cut -c20-25)
            rd_decimal=$((2#$rd)) # Convert binary to decimal
            ;;
        esac

    
    # Write to the log file
    {
        echo "State File: $(basename "$state_file")"
        echo "Seed: $seed"
        echo "Command Hex   : $current_command"
        echo "Command Binary: $current_command_binary"
        echo "opcode >$opcode<, funct3 >$funct3<, funct7 >$funct7<"
        echo "Command Name  : $command_name"
        if [ -n "$rd" ]; then
            echo "RD Register   : $rd_decimal"
        fi
        echo "Diff Lines:"
        echo "$diff_lines"
        echo ""
    } >> "$log_file"
done

echo "Log file created: $log_file"