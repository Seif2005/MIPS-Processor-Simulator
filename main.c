#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Define Instruction Memory Size (1024 * 16 bits = 1024 words, 16 bits per word)
#define INSTRUCTION_MEMORY_SIZE 1024
#define INSTRUCTION_MEMORY_WIDTH 16                   // 16 bits per word
uint16_t instruction_memory[INSTRUCTION_MEMORY_SIZE]; // Instruction memory (word-addressable)

// Define Data Memory Size (2048 * 8 bits = 2048 words, 8 bits per word)
#define DATA_MEMORY_SIZE 2048
#define DATA_MEMORY_WIDTH 8           // 8 bits per word (1 byte per word)
int8_t data_memory[DATA_MEMORY_SIZE]; // Data memory (byte/word addressable)
int skipped = 0;                      // Flag to indicate if the instruction was skipped

// At the top, define a NOP instruction value
#define NOP_INSTR 0xFFFF

// Function to load instruction into memory
void load_instruction(uint16_t address, uint16_t value)
{
    if (address < INSTRUCTION_MEMORY_SIZE)
    {
        instruction_memory[address] = value;
    }
    else
    {
        printf("Error: Instruction memory address out of range\n");
    }
}

// Function to load data into memory
void load_data(uint16_t address, uint8_t value)
{
    if (address < DATA_MEMORY_SIZE)
    {
        data_memory[address] = value;
    }
    else
    {
        printf("Error: Data memory address out of range\n");
    }
}

// Function to print an instruction memory block
void print_instruction_memory()
{
    printf("Instruction Memory (16-bit words):\n");
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++)
    {
        printf("Address %3d: 0x%04X\n", i, instruction_memory[i]);
    }
}

// Function to print a data memory block
void print_data_memory()
{
    printf("Data Memory (8-bit words):\n");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++)
    {
        printf("Address %4d: 0x%02X\n", i, data_memory[i]);
    }
}

// General Purpose Registers (R0 to R63): 8-bit each
#define NUM_GPRS 64
int8_t GPR[NUM_GPRS]; // R0 to R63

// Status Register (SREG): 8 bits (only 5 bits used)
typedef struct
{
    uint8_t C : 1;        // Carry Flag
    uint8_t V : 1;        // Overflow Flag
    uint8_t N : 1;        // Negative Flag
    uint8_t S : 1;        // Sign Flag (N XOR V)
    uint8_t Z : 1;        // Zero Flag
    uint8_t reserved : 3; // Bits 5-7 always 0
} SREG_t;

SREG_t SREG;

int8_t convert_6bit_twos_to_8bit(uint8_t value)
{
    // Mask to extract only the lower 6 bits
    uint8_t six_bit = value & 0b00111111;

    // Check if the sign bit (bit 5 of the 6-bit value) is set
    if (six_bit & 0b00100000)
    {
        // Negative number, sign-extend by setting bits 6 and 7
        return (six_bit | 0b11000000);
    }
    else
    {
        // Positive number
        return six_bit;
    }
}

int get_imm_value(int8_t imm){
    if (imm & 0b00100000)
    {
        // Negative number in 6-bit two's complement
        // First mask to 6 bits
        imm &= 0b00111111;
        // Convert to negative by sign-extending
        return -((~imm & 0b00111111) + 1);
    }
    else
    {
        // Positive number
        return imm & 0b00111111;
    }
}


int16_t convert8to16(int8_t input)
{
    // Since int8_t is already signed, assigning it to int16_t
    // automatically sign-extends it.
    int16_t result = (int16_t)input;
    return result;
}


void updateCarryFlag(SREG_t *sreg, uint8_t operand1, uint8_t operand2)
{
    uint16_t result = (uint16_t)operand1 + (uint16_t)operand2;
    sreg->C = (result > 0xFF) ? 1 : 0;
}

void updateOverflowFlag(SREG_t *sreg, int8_t operand1, int8_t operand2, int8_t result)
{
    int sign1 = (operand1 >> 7) & 1;
    int sign2 = (operand2 >> 7) & 1;
    int signR = (result >> 7) & 1;

    sreg->V = (sign1 == sign2) && (sign1 != signR);
}

void updateNegativeFlag(SREG_t *sreg, int8_t result)
{
    // Check bit 7 (the sign bit in 8-bit two's complement)
    if (result & 0b10000000)
    {
        sreg->N = 1;
    }
    else
    {
        sreg->N = 0;
    }
}

void updateSignFlag(SREG_t *sreg)
{
    sreg->S = sreg->N ^ sreg->V;
}

void updateZeroFlag(SREG_t *sreg, int8_t result)
{
    if (result == 0)
    {
        sreg->Z = 1;
    }
    else
    {
        sreg->Z = 0;
    }
}

// Program Counter (PC): 16-bit
uint16_t PC = 0;

// Helper macros to extract fields from a 16-bit instruction
#define OPCODE(instr) (((instr) >> 12) & 0b00001111)  // bits 15–12
#define R1_INDEX(instr) (((instr) >> 6) & 0b00111111) // bits 11–6
#define R2_INDEX(instr) ((instr) & 0b00111111)        // bits 5–0
#define IMM_VALUE(instr) ((instr) & 0b00111111)       // bits 5–0

int8_t extract_6bit_to_8bit(uint16_t instr, int is_unsigned) {
    int8_t imm6 = IMM_VALUE(instr);  // extract bits [5:0]
    if (!is_unsigned && (imm6 & 0b00100000)) {
        // For signed values, if sign bit (bit 5) is 1 → negative → sign-extend
        imm6 |= 0b11000000;  // Set bits 6 and 7 to 1
    }
    return imm6;
}

typedef struct
{
    uint8_t opcode;
    uint8_t r1;
    uint8_t r2;
    uint8_t immshift;
    int8_t imm;
} DecodedInstruction;

DecodedInstruction decode_instruction(uint16_t instruction)
{
    DecodedInstruction decoded;
    if (instruction == NOP_INSTR)
    {
        decoded.opcode = 0xFF; // NOP
        decoded.r1 = decoded.r2 = decoded.imm = 0;
        return decoded;
    }
    decoded.opcode = OPCODE(instruction);
    decoded.r1 = R1_INDEX(instruction);
    decoded.r2 = R2_INDEX(instruction);
    int immt = IMM_VALUE(instruction);
    if(decoded.opcode == 8 || decoded.opcode == 9){
        decoded.immshift = immt;
    }
    else{
        // Sign extend the immediate value
        if (immt & 0b00100000) {  // If bit 5 is set (negative)
            decoded.imm = immt | 0b11000000;  // Sign extend to 8 bits
        } else {
            decoded.imm = immt & 0b00111111;  // Keep only lower 6 bits
        }
    }
    return decoded;
}

// Executes a single instruction at PC and advances PC
void execute_instruction(DecodedInstruction instruction, uint16_t *IF_buffer_ptr, uint16_t *ID_buffer_ptr)
{
    /*
    uint8_t opcode = OPCODE(instruction);
    uint8_t r1 = R1_INDEX(instruction);
    uint8_t r2 = R2_INDEX(instruction);
    int8_t imm = IMM_VALUE(instruction); */

    // DecodedInstruction decoded = decode_instruction(instruction);

    uint8_t opcode = instruction.opcode;
    uint8_t r1 = instruction.r1;
    uint8_t r2 = instruction.r2;
    int8_t imm = instruction.imm;
    uint8_t immshift = instruction.immshift;
    int8_t result;


    // Convert imm to signed for all instructions except shift operations

    switch (opcode)
    {
    case 0: // ADD
        result = GPR[r1] + GPR[r2];
        updateCarryFlag(&SREG, (uint8_t)GPR[r1], (uint8_t)GPR[r2]);  // Use original values
        updateOverflowFlag(&SREG, GPR[r1], GPR[r2], result);
        GPR[r1] = result;  // Update register after flag calculations
        updateNegativeFlag(&SREG, result);
        updateZeroFlag(&SREG, result);
        updateSignFlag(&SREG);
        printf("ADD R%d = %d, C=%d, V=%d, N=%d, Z=%d, S=%d\n", r1, GPR[r1], SREG.C, SREG.V, SREG.N, SREG.Z, SREG.S);
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);
        break;

    case 1: // SUB
        result = GPR[r1] - GPR[r2];
        GPR[r1] = result;
        updateOverflowFlag(&SREG, GPR[r1], GPR[r2], result);
        updateNegativeFlag(&SREG, result);
        updateZeroFlag(&SREG, result);

        updateSignFlag(&SREG);
        printf("SUB R%d = %d, V=%d, N=%d, Z=%d, S=%d\n", r1, GPR[r1], SREG.V, SREG.N, SREG.Z, SREG.S);
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);

        break;

    case 2: // MUL
        result = GPR[r1] * GPR[r2];
        GPR[r1] = result;
        updateNegativeFlag(&SREG, result);
        updateZeroFlag(&SREG, result);
        printf("MUL R%d = %d, N=%d, Z=%d\n", r1, GPR[r1], SREG.N, SREG.Z);
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);
        break;

    case 3: // MOVI
        GPR[r1] = imm;  // Store the value (imm is already sign-extended)
        printf("MOVI R%d = %d\n", r1, GPR[r1]); // Print as signed
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);
        break;

    case 4: // BEQZ
        if (GPR[r1] == 0)
        {
            if (imm > 2)
            {
                skipped = 2; // Flush next 2 instructions
                PC = PC + imm - 2;
            }
            else
            {
                skipped = imm; // Flush next instruction
            }
            printf("BEQZ PC = %d (branch taken, pipeline flushed)\n", PC);
            printf("PC updated to %d in EX stage\n", PC);
        }
        else
        {
            printf("BEQZ not taken, continue normally.\n");
        }
        break;

    case 5: // ANDI
        result = GPR[r1] & imm;
        GPR[r1] = result;
        updateNegativeFlag(&SREG, result);
        updateZeroFlag(&SREG, result);
        printf("ANDI R%d = %d, N=%d, Z=%d\n", r1, GPR[r1], SREG.N, SREG.Z);
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);
        break;

    case 6: // EOR - Exclusive OR
        result = GPR[r1] ^ GPR[r2];
        GPR[r1] = result;
        updateNegativeFlag(&SREG, result);
        updateZeroFlag(&SREG, result);
        printf("EOR R%d = %d, N=%d, Z=%d\n", r1, GPR[r1], SREG.N, SREG.Z);
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);
        break;

    case 7: // BR (Branch Register)
        PC = (GPR[r1] << 8) | GPR[r2];
        *IF_buffer_ptr = NOP_INSTR;
        *ID_buffer_ptr = NOP_INSTR;
        printf("BR PC = %d (branch taken, pipeline flushed)\n", PC);
        printf("PC updated to %d in EX stage\n", PC);
        break;

    case 8:                               // SAL (Shift Left)
        result = GPR[r1] << (immshift); // Use unsigned 6 bits
        GPR[r1] = result;
        updateNegativeFlag(&SREG, result);
        updateZeroFlag(&SREG, result);
        printf("SAL R%d = %d, N=%d, Z=%d\n", r1, GPR[r1], SREG.N, SREG.Z);
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);
        break;

    case 9:                               // SAR (Shift Right)
        result = GPR[r1] >> (immshift); // Use unsigned 6 bits
        GPR[r1] = result;
        updateNegativeFlag(&SREG, result);
        updateZeroFlag(&SREG, result);
        printf("SAR R%d = %d, N=%d, Z=%d\n", r1, GPR[r1], SREG.N, SREG.Z);
        printf("Register R%d updated to %d in EX stage\n", r1, GPR[r1]);
        break;

    case 10: // LDR
        GPR[r1] = data_memory[imm];
        PC += 1;
        printf("LDR R%d = %d\n", r1, GPR[r1]);
        printf("Memory[%d] updated to %d in EX stage\n", imm, data_memory[imm]);
        break;

    case 11: // STR
        data_memory[imm] = GPR[r1];
        printf("STR mem[%d] = %d\n", imm, data_memory[imm]);
        printf("Memory[%d] updated to %d in EX stage\n", imm, data_memory[imm]);
        break;

    default:
        // Invalid opcode: halt or skip
        break;
    }

    printf("SREG updated: C=%d V=%d N=%d S=%d Z=%d in EX stage\n", SREG.C, SREG.V, SREG.N, SREG.S, SREG.Z);
    printf("PC updated to %d in EX stage\n", PC);
}
uint16_t fetch_instruction()
{
    if (PC < INSTRUCTION_MEMORY_SIZE && instruction_memory[PC] != 0)
        return instruction_memory[PC++];
    return NOP_INSTR;
}

// Pipeline Buffers for IF, ID, EX stages
uint16_t IF_buffer;           // Instruction Fetch buffer (up to 3 instructions)
uint16_t ID_buffer;           // Instruction Decode buffer (up to 3 instructions)
DecodedInstruction EX_buffer; // Execute buffer (up to 3 instructions)

// Add this helper function at file scope:
void print_instruction_human(uint16_t instr, const char *stage)
{
    if (instr == 0)
    {
        printf("  %s: (NOP)\n", stage);
        return;
    }
    DecodedInstruction d = decode_instruction(instr);
    const char *mnemonics[] = {"ADD", "SUB", "MUL", "MOVI", "BEQZ", "ANDI", "EOR", "BR", "SAL", "SAR", "LDR", "STR"};
    if (d.opcode > 11)
    {
        printf("  %s: (Invalid)\n", stage);
        return;
    }
    // Print based on instruction type
    switch (d.opcode)
    {
    case 0:
    case 1:
    case 2:
    case 6:
    case 7: // R-type: ADD, SUB, MUL, EOR, BR
        printf("  %s: %s R%d, R%d\n", stage, mnemonics[d.opcode], d.r1, d.r2);
        break;
    case 3:
    case 5:
    case 8:
    case 9: // I-type: MOVI, ANDI, SAL, SAR
        printf("  %s: %s R%d, %d\n", stage, mnemonics[d.opcode], d.r1, get_imm_value(d.imm));
        break;
    case 4: // BEQZ
        printf("  %s: %s R%d, %d\n", stage, mnemonics[d.opcode], d.r1,get_imm_value(d.imm));
        break;
    case 10:
    case 11: // LDR, STR
        printf("  %s: %s R%d, [%d]\n", stage, mnemonics[d.opcode], d.r1, get_imm_value(d.imm));
        break;
    default:
        printf("  %s: (Unknown)\n", stage);
    }
}

// get signed value of the immediate


// Run the pipeline
void run_pipeline()
{
    int remaining = INT32_MAX;
    skipped = 0;
    int cycle = 0;
    int n = 0; // Number of loaded instructions
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++)
    {
        if (instruction_memory[i] != 0)
            n++;
    }
    printf("initialized count is: %d\n", n);
    const DecodedInstruction dummy = {0xFF, 0, 0, 0}; // Dummy instruction for NOP
    // Initialize pipeline buffers to NOP
    IF_buffer = NOP_INSTR;
    ID_buffer = NOP_INSTR;
    EX_buffer = dummy; // Use 0xFF as NOP/invalid

    // Run for n+2 Instructions to account for the pipeline
    for (int cycle = 0; remaining > 0; cycle++)
    {
        // Shift EX and ID buffers
        EX_buffer = decode_instruction(ID_buffer);
        ID_buffer = IF_buffer;
        IF_buffer = fetch_instruction();
        if (IF_buffer == NOP_INSTR)
        {
            if (remaining == INT32_MAX)
            {
                remaining = 2;
            }
            remaining--;
        }

        // Shift pipeline: EX <- ID <- IF
        // Execute stage: execute EX_buffer[2]
        if (skipped > 0)
        {
            printf("Pipeline flushed due to branch. Skipping instruction.\n");
            skipped--;
            continue;
        }
        printf("\nCycle %d:\n", cycle + 1);
        print_instruction_human(IF_buffer, "IF");
        print_instruction_human(ID_buffer, "ID");
        DecodedInstruction ex_instr = EX_buffer;
        if (ex_instr.opcode == 0xFF)
        {
            printf("  EX: (NOP)\n");
        }
        else
        {
            uint16_t ex_raw = (ex_instr.opcode << 12) | (ex_instr.r1 << 6) | (ex_instr.opcode == 3 || ex_instr.opcode == 4 || ex_instr.opcode == 5 || ex_instr.opcode == 8 || ex_instr.opcode == 9 || ex_instr.opcode == 10 || ex_instr.opcode == 11 ? (ex_instr.imm & 0x3F) : (ex_instr.r2 & 0x3F));
            print_instruction_human(ex_raw, "EX");
            execute_instruction(ex_instr, &IF_buffer, &ID_buffer);
        }
    }

    printf("Execution complete. Final PC = 0x%04X\n", PC);

    printf("\nFinal Register Values:\n");
    for (int i = 0; i < NUM_GPRS; i++)
    {
        printf("R%d = %d\n", i, GPR[i]);
    }
    printf("PC = %d\n", PC);
    printf("SREG: C=%d V=%d N=%d S=%d Z=%d\n", SREG.C, SREG.V, SREG.N, SREG.S, SREG.Z);
    printf("\nInstruction Memory (nonzero):\n");
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++)
    {
        if (instruction_memory[i] != 0)
            printf("Addr %d: 0x%04X\n", i, instruction_memory[i]);
    }
    printf("\nData Memory (nonzero):\n");
    for (int i = 0; i < DATA_MEMORY_SIZE; i++)
    {
        if (data_memory[i] != 0)
            printf("Addr %d: 0x%02X\n", i, data_memory[i]);
    }
}
void resetAll()
{
    // Reset all states-----------------------WORK--------------------------------------------
    for (int i = 0; i < NUM_GPRS; i++)
        GPR[i] = 0;
    for (int i = 0; i < DATA_MEMORY_SIZE; i++)
        data_memory[i] = 0;
    for (int i = 0; i < INSTRUCTION_MEMORY_SIZE; i++)
        instruction_memory[i] = 0;
    PC = 0;
}

uint16_t parseOpcode(char opcode[])
{
    if (strcmp(opcode, "ADD") == 0)
        return 0x0000; // ADD
    if (strcmp(opcode, "SUB") == 0)
        return 0x1000; // SUB
    if (strcmp(opcode, "MUL") == 0)
        return 0x2000; // MUL
    if (strcmp(opcode, "MOVI") == 0)
        return 0x3000; // MOVI
    if (strcmp(opcode, "BEQZ") == 0)
        return 0x4000; // BEQZ
    if (strcmp(opcode, "ANDI") == 0)
        return 0x5000; // ANDI
    if (strcmp(opcode, "EOR") == 0)
        return 0x6000; // EOR
    if (strcmp(opcode, "BR") == 0)
        return 0x7000; // BR
    if (strcmp(opcode, "SAL") == 0)
        return 0x8000; // SAL
    if (strcmp(opcode, "SAR") == 0)
        return 0x9000; // SAR
    if (strcmp(opcode, "LDR") == 0)
        return 0xA000; // LDR
    if (strcmp(opcode, "STR") == 0)
        return 0xB000; // STR
    return -1;
}

uint16_t parseRegister(char reg[])
{
    // convert from string to int
    int regNum = atoi(reg + 1); // Skip the 'R' character
    // printf("Register number: %d\n", regNum);
    return 0x0000 | (regNum << 6); // Shift left by 6 bits to match the instruction format
}
uint16_t parseImmediate(char imm[])
{
    if (imm[0] == 'R')
    {
        // convert from string to int
        int regNum = atoi(imm + 1); // Skip the 'R' character
        return 0x0000 | (regNum); // Shift left by 6 bits to match the instruction format
    }
    // convert from string to int
    int immediate = atoi(imm);
    if (immediate < 0)
    {
        // Convert to 6-bit two's complement
        immediate = (~(-immediate) + 1) & 0x3F;
    }
    else
    {
        // Ensure positive numbers are also masked to 6 bits
        immediate &= 0x3F;
    }
    return 0x0000 | (immediate); // Shift left by 12 bits to match the instruction format
}

uint16_t parsefn(char line[])
{
    uint16_t hex = 0x0000;
    // Variables to store the extracted words
    char opcode[20] = {0};
    char reg[20] = {0};
    char imm[20] = {0};

    // Use sscanf to extract the three words
    sscanf(line, "%s %s %s", opcode, reg, imm);

    // Print the extracted components
    // printf("Opcode: %s\n", opcode);
    // printf("Register: %s\n", reg);
    // printf("Immediate/operand: %s\n", imm);
    hex = parseOpcode(opcode) |
          parseRegister(reg) |
          parseImmediate(imm);
    // printf("Hexadecimal: 0x%04X\n", hex);
    return hex;
}
int main()
{
    // to compile use: gcc -o main main.c
    resetAll();

    // Program:
    // MOVI R1, 5     => R1 = 5
    // MOVI R2, 3     => R2 = 3
    // ADD R1, R2     => R1 = R1 + R2 = 8
    // SUB R1, R2     => R1 = 8 - 3 = 5
    // MUL R1, R2     => R1 = 5 * 3 = 15
    // ANDI R1, 2     => R1 = 15 & 2 = 2
    // EOR R1, R2     => R1 = 2 ^ 3 = 1
    // BEQZ R1, +2    => R1 != 0 → skip
    // MOVI R3, 0     => This will be skipped if BEQZ is taken
    // MOVI R3, 7     => Executed if BEQZ not taken => R3 = 7
    // STR R3, [6]    => memory[6] = R3
    // LDR R4, [6]    => R4 = memory[6]
    // SAL R4, 1      => R4 = R4 << 1 = 14
    // SAR R4, 1      => R4 = R4 >> 1 = 7

    // instuction counter
    int counter = 0;
    char filename[100];
    char line[256];
    skipped = 0; // Reset skipped flag
    printf("Enter the file name: ");
    scanf("%99s", filename);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error opening file");
        return 55;
    }

    printf("\nFile Content:\n");
    while (fgets(line, sizeof(line), file) != NULL)
    {
        load_instruction(counter++, parsefn(line));
    }
    fclose(file);
    printf("\n"); // for clean output after last line

    run_pipeline();

    // load_instruction(0, 0x3045); // MOVI R1, 5 type:I
    // load_instruction(1, 0x3083); // MOVI R2, 3 type:I
    // load_instruction(2, 0x0042); // ADD R1, R2 type:R
    // load_instruction(3, 0x1042); // SUB R1, R2 type:R
    // load_instruction(4, 0x2042); // MUL R1, R2 type:R
    // load_instruction(5, 0x5042); // ANDI R1, 2 type:I
    // load_instruction(6, 0x6042); // EOR R1, R2 type:R
    // load_instruction(7, 0x4042); // BEQZ R1, +2 (imm = 2) type:I
    // load_instruction(8, 0x30C0); // MOVI R3, 0 (skipped if BEQZ taken) type:I
    // load_instruction(9, 0x30C7); // MOVI R3, 7 type:I
    // load_instruction(10, 0xB0C6); // STR R3, [6] type:I
    // load_instruction(11, 0xA106); // LDR R4, [6] type:I
    // load_instruction(12, 0x8101); // SAL R4, 1 type:I
    // load_instruction(13, 0x9101); // SAR R4, 1 type:I

    /*
        Cycle 1:
      IF: MOVI R1, 0
      ID: (NOP)
      EX: (NOP)

    Cycle 2:
      IF: MOVI R2, 5
      ID: MOVI R1, 0
      EX: (NOP)

    Cycle 3:
      IF: BR R1, R2
      ID: MOVI R2, 5
      EX: MOVI R1, 0
    MOVI R1 = 0

    Cycle 4:
      IF: ADD R3, R3
      ID: BR R1, R2
      EX: MOVI R2, 5
    MOVI R2 = 5

    Cycle 5:
      IF: SUB R3, R3
      ID: ADD R3, R3
      EX: BR R1, R2
    BR PC = 5 (branch taken, pipeline flushed)
    Pipeline flushed due to branch. Skipping instruction.

    Cycle 6:
      IF: MOVI R4, 9
      ID: SUB R3, R3
      EX: ADD R3, R3
    Pipeline flushed due to branch. Skipping instruction.

    Cycle 7:
      IF: ADD R4, R2
      ID: MOVI R4, 9
      EX: SUB R3, R3
    MOVI R4 = 9

    Cycle 8:
      IF: (NOP)
      ID: ADD R4, R2
      EX: MOVI R4, 9
    ADD R4 = 14, C=0, V=0, N=0, Z=0, S=0

    Cycle 9:
      IF: (NOP)
      ID: (NOP)
      EX: ADD R4, R2
    ADD R4 = 19, C=0, V=0, N=0, Z=0, S=0

    Execution complete. Final PC = 0x0007
    */

    return 0;
    // printf("%d\n", get_signed_imm(0x3F));  // Testing 0x3F = 111111 in binary = -1 in 6-bit 2's complement
    // printf("%d\n", get_signed_imm(0x20));  // Testing 0x20 = 100000 in binary = -32 in 6-bit 2's complement
    // printf("%d\n", get_signed_imm(0x01));  // Testing 0x01 = 000001 in binary = 1 in 6-bit 2's complement
    // printf("%d\n", get_signed_imm(0x1F));  // Testing 0x1F = 011111 in binary = 31 in 6-bit 2's complement
    // printf("%d\n", get_signed_imm(0x21));  // Testing 0x21 = 100001 in binary = -31 in 6-bit 2's complement
    // printf("%d\n", get_signed_imm(0x2F));  // Testing 0x2F = 101111 in binary = -17 in 6-bit 2's complement
}