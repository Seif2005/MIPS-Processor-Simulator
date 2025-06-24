# MIPS Processor Simulator

A simple command-line MIPS-like processor simulator written in C. This simulator provides instruction and data memory, a 3-stage pipeline (IF, ID, EX), basic flag and register logic, and simulates a small instruction set for educational and testing purposes.

## Features

- 3-stage pipeline: Instruction Fetch (IF), Instruction Decode (ID), Execute (EX)
- Instruction memory (1024 × 16 bits) and data memory (2048 × 8 bits)
- General Purpose Registers (GPRs)
- Status Register (SREG) with flags: Carry, Overflow, Negative, Sign, Zero
- Supports arithmetic, logical, branch, shift, load/store instructions
- Instruction loading from a text file
- Detailed cycle-by-cycle output for debugging and learning

## Supported Instructions

| Mnemonic | Description                        | Example            |
|----------|------------------------------------|--------------------|
| ADD      | Add registers                      | `ADD R1, R2`       |
| SUB      | Subtract registers                 | `SUB R1, R2`       |
| MUL      | Multiply registers                 | `MUL R1, R2`       |
| MOVI     | Move immediate to register         | `MOVI R1, 5`       |
| BEQZ     | Branch if equal to zero            | `BEQZ R1, +2`      |
| ANDI     | Bitwise AND with immediate         | `ANDI R1, 2`       |
| EOR      | Exclusive OR registers             | `EOR R1, R2`       |
| BR       | Branch to address in registers     | `BR R1, R2`        |
| SAL      | Shift register left (arithmetic)   | `SAL R1, 1`        |
| SAR      | Shift register right (arithmetic)  | `SAR R1, 1`        |
| LDR      | Load from memory to register       | `LDR R4, [6]`      |
| STR      | Store register to memory           | `STR R4, [6]`      |

## Getting Started

### Prerequisites

- GCC or any standard C compiler

### Building

Compile the simulator using:

```bash
gcc -o main main.c
```

### Running

Run the simulator executable:

```bash
./main
```

You will be prompted to enter the name of the file containing your instructions.

### Input File Format

- Each line should contain a single instruction in the format shown in the table above.
- Example `test.txt`:

  ```
  MOVI R1, 5
  MOVI R2, 3
  ADD R1, R2
  SUB R1, R2
  MUL R1, R2
  ANDI R1, 2
  EOR R1, R2
  BEQZ R1, +2
  MOVI R3, 0
  MOVI R3, 7
  STR R3, [6]
  LDR R4, [6]
  SAL R4, 1
  SAR R4, 1
  ```

### Example Usage

1. Prepare your instruction file, e.g., `test.txt` as above.
2. Run the simulator:

   ```bash
   ./main
   ```

3. When prompted, enter:

   ```
   test.txt
   ```

4. The simulator will load the instructions, run the pipeline, and display a cycle-by-cycle trace and the final register/memory state.

## Output

- Shows cycle-by-cycle pipeline status.
- Prints register and memory states after execution.

## Notes

- Instruction and data memory sizes are fixed.
- Only supported instruction mnemonics and formats are allowed.
- Immediate values and memory addresses are subject to size limits due to instruction width.

## License

MIT

## Author

[Seif2005](https://github.com/Seif2005)
