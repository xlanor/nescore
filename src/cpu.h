#ifndef CPU_H
#define CPU_H

#include <stdint.h>

// Status flag bits
#define FLAG_C 0x01  // Carry
#define FLAG_Z 0x02  // Zero
#define FLAG_I 0x04  // Interrupt Disable
#define FLAG_D 0x08  // Decimal (unused on NES but exists)
#define FLAG_B 0x10  // Break
#define FLAG_U 0x20  // Unused (always 1)
#define FLAG_V 0x40  // Overflow
#define FLAG_N 0x80  // Negative

#define STACK_BASE 0x0100

typedef struct {
    uint16_t pc;     // Program counter
    uint8_t  sp;     // Stack pointer
    uint8_t  a;      // Accumulator
    uint8_t  x;      // X register
    uint8_t  y;      // Y register
    uint8_t  status; // Status flags
    uint64_t cycles; // Total cycle count
} CPU;

void cpu_init(CPU *cpu);
void cpu_reset(CPU *cpu);
void cpu_step(CPU *cpu);

#endif
