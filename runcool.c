//  CITS2002 Project 1 2021
//  Name(s):             Kane Howard
//  Student number(s):   22253494

//  compile with:  cc -std=c11 -Wall -Werror -o runcool runcool.c

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//  THE STACK-BASED MACHINE HAS 2^16 (= 65,536) WORDS OF MAIN MEMORY
#define N_MAIN_MEMORY_WORDS (1<<16)

//  EACH WORD OF MEMORY CAN STORE A 16-bit UNSIGNED ADDRESS (0 to 65535)
#define AWORD               uint16_t
//  OR STORE A 16-bit SIGNED INTEGER (-32,768 to 32,767)
#define IWORD               int16_t

//  THE ARRAY OF 65,536 WORDS OF MAIN MEMORY
AWORD                       main_memory[N_MAIN_MEMORY_WORDS];

//  THE SMALL-BUT-FAST CACHE HAS 32 WORDS OF MEMORY
#define N_CACHE_WORDS       32


//  see:  https://teaching.csse.uwa.edu.au/units/CITS2002/projects/coolinstructions.php
enum INSTRUCTION {
    I_HALT       = 0,
    I_NOP,
    I_ADD,
    I_SUB,
    I_MULT,
    I_DIV,
    I_CALL,
    I_RETURN,
    I_JMP,
    I_JEQ,
    I_PRINTI,
    I_PRINTS,
    I_PUSHC,
    I_PUSHA,
    I_PUSHR,
    I_POPA,
    I_POPR
};

//  USE VALUES OF enum INSTRUCTION TO INDEX THE INSTRUCTION_name[] ARRAY
const char *INSTRUCTION_name[] = {
    "halt",
    "nop",
    "add",
    "sub",
    "mult",
    "div",
    "call",
    "return",
    "jmp",
    "jeq",
    "printi",
    "prints",
    "pushc",
    "pusha",
    "pushr",
    "popa",
    "popr"
};

//  ----  IT IS SAFE TO MODIFY ANYTHING BELOW THIS LINE  --------------


//  THE STATISTICS TO BE ACCUMULATED AND REPORTED
int n_main_memory_reads     = 0;
int n_main_memory_writes    = 0;
int n_cache_memory_hits     = 0;
int n_cache_memory_misses   = 0;

void report_statistics(void)
{
    printf("@number-of-main-memory-reads-fast-jeq\t%i\n",    n_main_memory_reads);
    printf("@number-of-main-memory-writes-fast-jeq\t%i\n",   n_main_memory_writes);
    printf("@number-of-cache-memory-hits          \t%i\n",    n_cache_memory_hits);
    printf("@number-of-cache-memory-misses        \t%i\n",  n_cache_memory_misses);
}

//  -------------------------------------------------------------------

//  EVEN THOUGH main_memory[] IS AN ARRAY OF WORDS, IT SHOULD NOT BE ACCESSED DIRECTLY.
//  INSTEAD, USE THESE FUNCTIONS read_memory() and write_memory()
//
//  THIS WILL MAKE THINGS EASIER WHEN WHEN EXTENDING THE CODE TO
//  SUPPORT CACHE MEMORY

/*

-- CACHE INFO --

For my cache I have used a few different arrays to hold various values at certain cache indexes.
The cache index is determined by the 5 Least Significant Bits.

cache_tag -- Holds the tag at the cache index (Uses the 11 Most Significant Bits of the address)
dirtyBit -- 1 or 0 depending if the cache index is dirty or not
cache_memory -- This is the actual cache memory and the values it holds

*/

bool dirtyBit[N_CACHE_WORDS];

int cache_tag[N_CACHE_WORDS]; // 11 MSBs

AWORD cache_memory[N_CACHE_WORDS]; // contains at most 32 AWORDs

AWORD previousAddress; // for writing back to the previous address


AWORD read_memory(AWORD address)
{
    // number from 0 - 31
    // Just helps us locate which line of cache to check
    int index = address & 0x1f; // 5 LSBs

    unsigned mask = 0b0000011111111111;
    int tag = ((address>>5) & mask); // 11 MSBs

    AWORD previousValue;

    // --- LOGIC FOR CACHE ---

    // Cache Hit
    if (cache_tag[index] == tag) {

        ++n_cache_memory_hits;

        return cache_memory[index]; // return value in cache

    // Cache Miss
    } else {

        n_cache_memory_misses++;

        // MEM & CACHE out of sync -- Write-back required
        if (dirtyBit[index] == true) {

            previousAddress = (cache_tag[index] << 5) | index;

            ++n_main_memory_writes;
            
            main_memory[previousAddress] = cache_memory[index];
        }

        ++n_main_memory_reads;

        // Write into cache the data at the memroy address
        cache_memory[index] = main_memory[address];

        // tag is remembered here for the corresponding cache index
        cache_tag[index] = tag;

        // dirty bit is set to false
        dirtyBit[index] = false;

        // return what is in cache memory at corresponding index
        return cache_memory[index];

        }
}

void write_memory(AWORD address, AWORD value)
{
    int index = address & 0x1f;

    unsigned mask = 0b0000011111111111;
    int tag = ((address>>5) & mask); // 11 MSBs

    AWORD previousValue;

    // --- CACHE LOGIC --- 

    // CACHE HIT
    if (cache_tag[index] == tag) {

        cache_memory[index] = value; // Update value in cache line

        dirtyBit[index] = true; // cache and main memory now out of sync

    // CACHE MISS
    } else {

        // write back its previous data into main memory
        if (dirtyBit[index] == true) {

            previousAddress = (cache_tag[index] << 5) | index;

            ++n_main_memory_writes;

            main_memory[previousAddress] = cache_memory[index];

        }

        // Update tag at the cache index
        cache_tag[index] = tag;

        // Update value in cache index
        cache_memory[index] = value;

        // Value in cache is now out of sync with main memory
        dirtyBit[index] = true;

    }
}

//  -------------------------------------------------------------------

// Function that checks for a negative relative FP address
int checkNegative(int FP_value, int value) {

    if ((FP_value + value) > 65536) {
        int value3 = value - 65536;
        return FP_value + value3;
    } else {
        return (value + FP_value);
    }
}

//  EXECUTE THE INSTRUCTIONS IN main_memory[]
int execute_stackmachine(void)
{

//  THE 3 ON-CPU CONTROL REGISTERS:
    int PC      = 0;                    // 1st instruction is at address=0
    int SP      = N_MAIN_MEMORY_WORDS;  // initialised to top-of-stack Goes DOWN
    int FP      = 0;                    // frame pointer

    while(true) {
        AWORD value1;
        AWORD value2;
        AWORD returnAddress;

//  FETCH THE NEXT INSTRUCTION TO BE EXECUTED

        IWORD instruction   = read_memory(PC); 
        ++PC;

        if(instruction == I_HALT) {
            break;
        }
        switch (instruction) {
        case I_NOP :
                        break;

        case I_ADD :
                        value1 = read_memory(SP++);
                        value2 = read_memory(SP);
                        write_memory(SP, value2 + value1);
                        break;

        case I_SUB :
                        value1 = read_memory(SP++);
                        value2 = read_memory(SP);
                        write_memory(SP, value2 - value1);
                        break;

        case I_MULT :
                        value1 = read_memory(SP++);
                        value2 = read_memory(SP);
                        write_memory(SP, value2 * value1);
                        break;

        case I_DIV :
                        value1 = read_memory(SP++);
                        value2 = read_memory(SP);
                        write_memory(SP, value2 / value1);
                        break;

        case I_CALL :

                        // STEP 1 -=-=
                        // Save the address of the next function starting address

                        value1 = read_memory(PC);
                        ++PC; // PC now points to the return address


                        // STEP 2 -=-=
                        // Write the return address to TOS
                        // To return to after completion of the upcoming function

                        --SP;
                        write_memory(SP, PC); // Return addr is pushed to TOS


                        // STEP 3 -=-=
                        // Current value of FP is saved onto the stack (Soon to be previous FP)

                        --SP;
                        write_memory(SP, FP);


                        // STEP 4 -=-=
                        // TOS is copied into FP skip

                        FP = SP;

                        // STEP 5 -=-=
                        // Program Counter moves to first instruction of the function

                        PC = value1;

                        break;

        case I_RETURN :

                        //  STEP 1 -=-=
                        //  Save the return address incase of overwrite
                        // Next instruction is fpoffset to put return value

                        returnAddress = read_memory(FP + 1);

                        AWORD fpOffset = read_memory(PC);

                        // STEP 2 -=-=
                        // Return value (Which is on TOS) is returned to top of callers frame

                        AWORD returnValue = read_memory(SP); // Get return value
                        ++SP;

                        write_memory(FP + fpOffset, returnValue); // Write to top of callers frame

                        // STEP 3 -=-=
                        // SP must be adjusted so it is at TOS of callers FRAME

                        SP = (FP + fpOffset);

                        // STEP 4 -=-=
                        // PC is set to the return address

                        PC = returnAddress;

                        // STEP 5 -=-=
                        // CHANGE FP to previousFP

                        FP = read_memory(FP);

                        break;

        case I_JMP : 
                        PC = read_memory(PC);
                        break;

        case I_JEQ :
                        value1 = read_memory(SP++);
                        if (value1 == 0) {
                            PC = read_memory(PC);
                        } else {
                            ++PC;
                        }
                        break;

        case I_PRINTI :
                        printf("%i", read_memory(SP++));
                        break;

        case I_PRINTS :

                        value1 = read_memory(PC); // value1 is the address of the string

                        while (true) {

                            int allBits = read_memory(value1); // 16 bits of data is retrieved

                            unsigned  mask;
                            mask = (1 << 8) - 1;
                            char splitBits = allBits & mask;

                            // If null-byte is found - break from while loop
                            if (splitBits == '\0') {
                                break;
                            }

                            printf("%c", splitBits);

                            allBits = (allBits >> 8); // shift to give us right most 8 bits

                            mask = (1 << 8) - 1;
                            splitBits = allBits & mask;

                            // If null-byte is found - break from while loop
                            if (splitBits == '\0') {
                                break;
                            }

                            printf("%c", splitBits);

                            value1++;
                        }

                        ++PC;

                        break;

        case I_PUSHC : 
                        write_memory(--SP, read_memory(PC++));

                        break;

        case I_PUSHA : 
                        write_memory(--SP, read_memory(read_memory(PC++)));

                        break;

        case I_PUSHR :
                        write_memory(--SP, read_memory(checkNegative(FP, read_memory(PC++))));

                        break;

        case I_POPA : 
                        write_memory(read_memory(PC++), read_memory(SP++));

                        break;

        case I_POPR :   
                        write_memory(checkNegative(FP, read_memory(PC++)), read_memory(SP++));
                    
                        break;
        }
    }

//  THE RESULT OF EXECUTING THE INSTRUCTIONS IS FOUND ON THE TOP-OF-STACK
    return (IWORD) read_memory(SP);
}

//  -------------------------------------------------------------------

// Set all dirty bits to dirty + set all tags to 2047
void warmCache() {

  for (int i = 0; i < N_CACHE_WORDS; i++) {
    cache_tag[i] = 2047;
    dirtyBit[i] = true;
  }
}

//  READ THE PROVIDED coolexe FILE INTO main_memory[]
void read_coolexe_file(char filename[])
{

    memset(main_memory, 0, sizeof main_memory);   //  clear all memory


//  READ CONTENTS OF coolexe FILE
    FILE *fp = fopen(filename, "rb");

    fread(main_memory, sizeof (AWORD), N_MAIN_MEMORY_WORDS, fp);

    fclose(fp);

    // Initialise cache for use
    warmCache();

}

//  -------------------------------------------------------------------

int main(int argc, char *argv[])
{
//  CHECK THE NUMBER OF ARGUMENTS
    if(argc != 2) {
        fprintf(stderr, "Usage: %s program.coolexe\n", argv[0]);
        exit(EXIT_FAILURE);
    }

//  READ THE PROVIDED coolexe FILE INTO THE EMULATED MEMORY
    read_coolexe_file(argv[1]);

//  EXECUTE THE INSTRUCTIONS FOUND IN main_memory[]
    int result = execute_stackmachine();

    printf("Exit = %i\n", result);

    report_statistics();

    return result;          // or  exit(result);
}
