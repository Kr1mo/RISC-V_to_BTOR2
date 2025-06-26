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

# if log file already exists, make a copy
if [ -f "$log_file" ]; then
    cp "$log_file" "${log_file}.bak"
fi

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
            "000") command_name="LoadB" ;;  # Load Byte
            "001") command_name="LoadH" ;;  # Load Halfword
            "010") command_name="LoadW" ;;  # Load Word
            "011") command_name="LoadD" ;;  # Load Doubleword
            "100") command_name="LoadBU" ;; # Load Byte Unsigned
            "101") command_name="LoadHU" ;; # Load Halfword Unsigned
            "110") command_name="LoadWU" ;; # Load Word Unsigned
            *) command_name="Unknown Load-type" ;;
            esac
            ;;
        "0100011") # Store instructions
            case "$funct3" in
            "000") command_name="StoreB" ;;  # Store Byte
            "001") command_name="StoreH" ;;  # Store Halfword
            "010") command_name="StoreW" ;;  # Store Word
            "011") command_name="StoreD" ;;  # Store Doubleword
            *) command_name="Unknown Store-type" ;;
            esac
            ;;
        "1100011") # Branch instructions
            case "$funct3" in
                "000") command_name="BEQ" ;;  # Branch if Equal
                "001") command_name="BNE" ;;  # Branch if Not Equal
                "100") command_name="BLT" ;;  # Branch if Less Than
                "101") command_name="BGE" ;;  # Branch if Greater or Equal
                "110") command_name="BLTU" ;; # Branch if Less Than Unsigned
                "111") command_name="BGEU" ;; # Branch if Greater or Equal Unsigned
                *) command_name="Unknown Branch-type" ;;
            esac
            ;;
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
            rd=$(echo "$current_command_binary" | cut -c21-25)
            rd_decimal=$((2#$rd)) # Convert binary to decimal
            ;;
        esac
    
    # Extract the rs1 register if applicable
    rs1=""
    case "$opcode" in
        "0110111"|"0010111"|"1101111") # Instructions that not use rs1 (LUI, AUIPC, JAL)
            ;;
        *)
            rs1=$(echo "$current_command_binary" | cut -c12-17)
            rs1_decimal=$((2#$rs1)) # Convert binary to decimal
            ;;
    esac
    # Extract the rs2 register if applicable, only for R-type, W-type, B-type and store instructions
    rs2=""
    case "$opcode" in
        "0110011"|"0111011"|"0100011"|"1100011") # Instructions that use rs2
            rs2=$(echo "$current_command_binary" | cut -c1-5)
            rs2_decimal=$((2#$rs2)) # Convert binary to decimal
            ;;
    esac

    # Extract immediate value if applicable
    immediate=""
    case "$opcode" in
        "0010011"|"0000011"|"1100111"|"0011011") # I-type, Load, JALR, IW-type instructions
            immediate=$(echo "$current_command_binary" | cut -c1-12) # Extract immediate value
            ;;
        "0100011") # S-type instructions
            immediate=$(echo "$current_command_binary" | cut -c1-7) # Extract immediate value
            immediate+="$(echo "$current_command_binary" | cut -c21-25)" # Combine the two parts of the immediate
            ;;
        "1100011") # B-type instructions
            immediate=$(echo "$current_command_binary" | cut -c1) # Extract immediate value
            immediate+="$(echo "$current_command_binary" | cut -c25)" # Combine the two parts of the immediate
            immediate+="$(echo "$current_command_binary" | cut -c2-7)" # Combine the next 6 bits
            immediate+="$(echo "$current_command_binary" | cut -c21-25)" # Combine the next 4 bits
            immediate+="0" # Add the last bit for B-type instructions
            ;;
        "0110111" | "0010111") # U-type instructions LUI and AUIPC
            immediate=$(echo "$current_command_binary" | cut -c1-20) # Extract the 20-bit immediate value
            immediate+="000000000000" # Pad with zeros to make it 32 bits
            ;;
        "1101111") # J-type instruction JAL
            immediate=$(echo "$current_command_binary" | cut -c1) # Extract immediate value
            immediate+="$(echo "$current_command_binary" | cut -c13-20)" # Combine the next 8 bits
            immediate+="$(echo "$current_command_binary" | cut -c12)" # Combine the 1 bit
            immediate+="$(echo "$current_command_binary" | cut -c2-11)" # Combine the next 10 bits
            immediate+="0" # Add the last bit for J-type instructions
            ;;
    esac

    register_concat=""
    if [ -n "$rd" ]; then
        register_concat+="RD: $rd_decimal "
    fi
    if [ -n "$rs1" ]; then
        rs1_decimal=$((2#$rs1)) # Convert binary to decimal
        register_concat+="; RS1: $rs1_decimal "
    fi
    if [ -n "$rs2" ]; then
        rs2_decimal=$((2#$rs2)) # Convert binary to decimal
        register_concat+="; RS2: $rs2_decimal "
    fi

    # If a backup of the log file exists, keep the notes from it
    notes=""
    if [ -f "${log_file}.bak" ]; then
        notes=$(grep "$(basename "$state_file" .state)_notes:" "${log_file}.bak")
    fi
    # If notes exist, remove the prefix
    if [ -n "$notes" ]; then
        notes=$(echo "$notes" | sed 's/.*_notes: //')
    fi
     
    # Write to the log file
    {
        echo "State File: $(basename "$state_file")"
        echo "Seed: $seed"
        echo "Command Hex   : $current_command"
        echo "Command Binary: $current_command_binary"
        echo "opcode >$opcode<, funct3 >$funct3<, funct7 >$funct7<"
        echo ""
        echo "Command Name  : $command_name"
        if [ -n "$register_concat" ]; then
            echo "Registers     : $register_concat"
        fi
        echo ""
        if [ -n "$immediate" ]; then
            immediate_decimal=$((2#$immediate)) # Convert binary immediate to decimal
            immediate_hex=$(printf "%X" $immediate_decimal) # Convert decimal immediate to hexadecimal
            echo "Immediate     : $immediate"
            echo "Immediate Hex : $immediate_hex (Decimal: $immediate_decimal)"
        fi
        echo ""
        echo "Diff Lines:"
        echo "$diff_lines"
        echo ""
        echo "$(basename "$state_file" .state)_notes: $notes"
        echo ""
        echo "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        echo ""
    } >> "$log_file"
done

# If backup exists, remove it
if [ -f "${log_file}.bak" ]; then
    rm "${log_file}.bak"
fi

echo "Log file created: $log_file"