#include "cpu.h"
#include "bus.h"
#include <stdbool.h>
#include <stdio.h>

static void set_zn(CPU *cpu, uint8_t val) {
    if (val == 0) cpu->status |= FLAG_Z; else cpu->status &= ~FLAG_Z;
    if (val & 0x80) cpu->status |= FLAG_N; else cpu->status &= ~FLAG_N;
}
static uint16_t read16(CPU *cpu) {
    uint8_t lo = bus_read(cpu->pc ++);
    uint8_t hi = bus_read(cpu->pc ++);
    return (hi << 8) | lo;
}

static bool page_crossed(uint16_t a, uint16_t b) {
    return (a & 0xFF00) != (b & 0xFF00);
}

static uint16_t addr_imm(CPU *cpu) { return cpu->pc++; }
static uint16_t addr_zpg(CPU *cpu) { return bus_read(cpu->pc++); }
static uint16_t addr_zpx(CPU *cpu) { return (bus_read(cpu->pc++) + cpu->x) & 0xFF; }
static uint16_t addr_abs(CPU *cpu) { return read16(cpu); }
static uint16_t addr_abx(CPU *cpu, bool *crossed) {
    uint16_t base = read16(cpu);
    uint16_t addr = base + cpu->x;
    *crossed = page_crossed(base, addr);
    return addr;
}
static uint16_t addr_aby(CPU *cpu, bool *crossed) {
    uint16_t base = read16(cpu);
    uint16_t addr = base + cpu->y;
    *crossed = page_crossed(base, addr);
    return addr;
}
static uint16_t addr_izx(CPU *cpu) {
    uint8_t zp = (bus_read(cpu->pc++) + cpu->x) & 0xFF;
    return bus_read(zp) | (bus_read((zp + 1) & 0xFF) << 8);
}
static uint16_t addr_izy(CPU *cpu, bool *crossed) {
    uint8_t zp = bus_read(cpu->pc++);
    uint16_t base = bus_read(zp) | (bus_read((zp + 1) & 0xFF) << 8);
    uint16_t addr = base + cpu->y;
    *crossed = page_crossed(base, addr);
    return addr;
}

void cpu_init(CPU *cpu) {
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->sp = 0xFD;
    cpu->status = FLAG_U | FLAG_I;
    cpu->pc = 0;
    cpu->cycles = 0;
}

void cpu_reset(CPU *cpu) {
    uint16_t lo = bus_read(0xFFFC);
    uint16_t hi = bus_read(0xFFFD);
    cpu->pc = (hi << 8) | lo;
    cpu->sp = 0xFD;
    cpu->status = FLAG_U | FLAG_I;
    cpu->cycles = 7;
}


void cpu_step(CPU *cpu) {
    uint8_t opcode = bus_read(cpu->pc++);

    switch (opcode) {
        //https://www.masswerk.at/6502/6502_instruction_set.html
        /*
         * LDA - Load Accumulator with Memory
         * M -> A                          N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       LDA #oper        A9    2      2
         * zeropage        LDA oper         A5    2      3
         * zeropage,X      LDA oper,X       B5    2      4
         * absolute        LDA oper         AD    3      4
         * absolute,X      LDA oper,X       BD    3      4*
         * absolute,Y      LDA oper,Y       B9    3      4*
         * (indirect,X)    LDA (oper,X)     A1    2      6
         * (indirect),Y    LDA (oper),Y     B1    2      5*
         */
        case 0xA9:{
            cpu->a = bus_read(addr_imm(cpu));
            cpu->cycles += 2;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0xA5:{
            cpu->a = bus_read(addr_zpg(cpu));
            cpu->cycles += 3;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0xB5:{
            cpu->a = bus_read(addr_zpx(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0xAD:{
            cpu->a = bus_read(addr_abs(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0xBD:{
            bool crossed;
            cpu->a = bus_read(addr_abx(cpu, &crossed));
            cpu->cycles += 4;
            if(crossed) {
                cpu->cycles += 1;
            }
            set_zn(cpu, cpu->a);
            break;
        }
        case 0xB9:{
            bool crossed;
            cpu->a = bus_read(addr_aby(cpu, &crossed));
            cpu->cycles += 4;
            if(crossed) {
                cpu->cycles += 1;
            }
            set_zn(cpu, cpu->a);
            break;
        }
        case 0xA1: {
            cpu ->a = bus_read(addr_izx(cpu));
            cpu->cycles += 6;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0xB1: {
            bool crossed;
            cpu ->a = bus_read(addr_izy(cpu,&crossed));
            cpu->cycles += 6;
            if(crossed) {
                cpu->cycles += 1;
            }
            set_zn(cpu, cpu->a);
            break;
        }  
    default:
        fprintf(stderr, "Unknown opcode: %02X at %04X\n", opcode, cpu->pc - 1);
        break;
    }
}
