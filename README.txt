WHAT TO KNOW ABOUT CHIP-8?

Compact Hexadecimal Interpretive Programming language designed for 8-bit microcomputers (CHIP-8), USED 8 BIT REGISTERS

MEMORY SHOULD BE 4096 BYTES, CHIP-8's INDEX REGISTER AND PROGRAM COUNTER CAN ONLY ADDRESS 12 BITS (WHICH IS 4096 ADDRESSES)
    - A PROGRAM COUNTER IS A CPU REGISTER THAT STORES THE MEMORY OF THE NEXT INSTRUCTION TO BE EXECUTED (AUTOMATICALLY INCREMENTS TO KEEP PROGRAM EXECUTION SEQUENTIAL)
    - AN INDEX REGISTER IS A PROCESSOR REGISTER (CPU's SMALL AND FAST WORKING MEMORY) USED TO HELP COMPUTE OPERAND ADDRESSES (LOCATION IN MEMOERY) DURING INDEXED ADDRESSING 
      (BASE ADDRESS (The memory address of the start of a data structure, EXAMPLE: START OF AN ARRAY) + INDEX REGISTER (the offset, EXAMPLE: how far you want to go in the array )), 
      MAINLY FOR ACCESSING ARRAYS OR BLOCKS OF DATA
    
    - IN PLAIN ENGLISH:
      Base address = “start of data”
      Index register = “how far to move”
      Adding them = “where exactly the data is” (INDEXED ADDRESSING)

In CHIP-8, all program memory is stored in RAM, and programs are loaded into this memory at runtime. Although CHIP-8 binaries are commonly referred to as “ROMs” 
this is a convention used by emulators. Unlike true ROM (such as console cartridges), CHIP-8 programs reside in writable memory and can technically modify themselves,
although this behavior is not typically relied upon


FONT:

THE CHIP-8 EMULATOR SHOULD HAVE A BUILT-IN FONT WITH SPRITE DATA REPRESENTING HEXADECIMAL NUMBERS FROM 0 TO F
EACH FONT CHARACTER SHOULD BE 4 PIXELS WIDE BY 5 PIXELS TALL

The font most people use is represented in bytes like this:

0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
0x20, 0x60, 0x20, 0x20, 0x70, // 1
0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
0x90, 0x90, 0xF0, 0x10, 0x10, // 4
0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
0xF0, 0x10, 0x20, 0x40, 0x40, // 7
0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
0xF0, 0x90, 0xF0, 0x90, 0x90, // A
0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
0xF0, 0x80, 0x80, 0x80, 0xF0, // C
0xE0, 0x90, 0x90, 0x90, 0xE0, // D
0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
0xF0, 0x80, 0xF0, 0x80, 0x80  // F

DISPLAY:

DISPLAY IS 64 PIXELS WIDE BY 32 PIXELS TALL, EACH PIXEL IS EITHER ON OR OFF (BOOLEAN VALUE)

This method of drawing will inevitable cause some flickering objects; 
when a sprite is moved, it’s first erased from the screen (by simply drawing it again, flipping all its lit pixels) and then re-drawn in the new position, 
so it will disappear for a little while, often causing a flickering effect. 
There are ways to mitigate this. For example, pixels that are erased could fade out instead of disappearing completely.

STACK:

CHIP-8 has a stack (a common “last in, first out” data structure where you can either “push” data to it or “pop” the last piece of data you pushed) 
CHIP-8 uses it to call and return from subroutines (“functions”) and nothing else, so save addresses there
These original interpreters had limited space on the stack, usually at least 16 two-byte entries. You can limit the stack likewise, or just keep it unlimited
CHIP-8 programs usually don’t nest subroutine calls too much since the stack was so small originally, so it doesn’t really matter

TIMERS:

There are two seperate timer registers: 
  -Delay timer
  -Sound timer
Both of these work the same (one byte in size, and as long as their value is above 0 they should be decremented by one 60 times per second, ie. at 60hz)

FETCH/DECODE/EXECUTE LOOP:

An emulator’s main task is simple. It runs in an infinite loop, and does these three tasks in succession:

  -Fetch the instruction from memory at the current PC (program counter)
  -Decode the instruction to find out what the emulator should do
  -Execute the instruction and do what it tells you


16 8-bit (one byte) general-purpose variable registers numbered 0 through F hexadecimal, ie. 0 through 15 in decimal, called V0 through VF
VF is also used as a flag register; many instructions will set it to either 1 or 0 based on some rule, for example using it as a carry flag and a collision detector 
for graphics

Opcodes (operation codes) are the fundamental machine-readable instructions that tell a CPU or virtual machine (like the EVM or Java VM) exactly what action to 
perform, such as adding, subtracting, moving data, or jumping between commands. The CPU works by fetching 2-byte instructions (opcodes) and decoding them. 
There are only about 35 instructions to implement.


RESOURCES:
  - https://tobiasvl.github.io/blog/write-a-chip-8-emulator/
  - https://en.wikipedia.org/wiki/CHIP-8#Opcode_table