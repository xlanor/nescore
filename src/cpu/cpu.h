#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "../bus.h"

// Status flag bits
#define FLAG_C 0x01  // Carry
#define FLAG_Z 0x02  // Zero
#define FLAG_I 0x04  // Interrupt Disable
#define FLAG_D 0x08  // Decimal (unused on NES but exists)
#define FLAG_B 0x10  // Break
#define FLAG_U 0x20  // Unused (always 1)
#define FLAG_V 0x40  // Overflow
#define FLAG_N 0x80  // Negative

#define BIT0 0x01
#define BIT7 0x80

#define STACK_BASE 0x0100

typedef struct {
    // Registers
    uint16_t pc;
    uint8_t  sp;
    uint8_t  a; 
    uint8_t  x;
    uint8_t  y;
    uint8_t  status;
    uint64_t cycles; 

    // FSM 
    uint8_t  opcode;
    uint8_t  cycle; 
    uint16_t addr;
    uint8_t  data;

    Bus *bus;
} CPU;

void cpu_init(CPU *cpu);
void cpu_reset(CPU *cpu);
void cpu_step(CPU *cpu);
void cpu_tick(CPU *cpu);
void cpu_trace(CPU *cpu);

#endif
