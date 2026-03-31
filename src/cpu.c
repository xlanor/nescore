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
static uint16_t addr_zpy(CPU *cpu) { return (bus_read(cpu->pc++) + cpu->y) & 0xFF; }
static uint16_t addr_abs(CPU *cpu) { return read16(cpu); }
static uint16_t addr_abx(CPU *cpu, bool *crossed) {
    uint16_t base = read16(cpu);
    uint16_t addr = base + cpu->x;
    if (crossed) *crossed = page_crossed(base, addr);
    return addr;
}
static uint16_t addr_aby(CPU *cpu, bool *crossed) {
    uint16_t base = read16(cpu);
    uint16_t addr = base + cpu->y;
    if (crossed) *crossed = page_crossed(base, addr);
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
    if (crossed) *crossed = page_crossed(base, addr);
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
        // All comments from the tables here:
        // https://www.masswerk.at/6502/6502_instruction_set.html
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
        /*
         * STA - Store Accumulator in Memory
         * A -> M                          N Z C I D V
         *                                 - - - - - -
         * addressing      assembler       opc  bytes  cycles
         * zeropage        STA oper         85    2      3
         * zeropage,X      STA oper,X       95    2      4
         * absolute        STA oper         8D    3      4
         * absolute,X      STA oper,X       9D    3      5
         * absolute,Y      STA oper,Y       99    3      5
         * (indirect,X)    STA (oper,X)     81    2      6
         * (indirect),Y    STA (oper),Y     91    2      6
         */
        case 0x85: {
            bus_write(addr_zpg(cpu), cpu->a);
            cpu->cycles += 3;
            break; 
        }
        case 0x95: {
            bus_write(addr_zpx(cpu), cpu->a);
            cpu->cycles += 4;
            break; 
        }
        case 0x8D: {
            bus_write(addr_abs(cpu), cpu->a);
            cpu->cycles += 4;
            break; 
        }
        case 0x9D: {
            bus_write(addr_abx(cpu, NULL), cpu->a);
            cpu->cycles += 5;
            break; 
        }
        case 0x99: {
            bus_write(addr_aby(cpu, NULL), cpu->a);
            cpu->cycles += 5;
            break; 
        }
        case 0x81: {
            bus_write(addr_izx(cpu), cpu->a);
            cpu->cycles += 6;
            break; 
        }
        case 0x91: {
            bus_write(addr_izy(cpu,NULL), cpu->a);
            cpu->cycles += 6;
            break; 
        }
        /*
         * STX - Store Index X in Memory
         * X -> M                          N Z C I D V
         *                                 - - - - - -
         * addressing      assembler       opc  bytes  cycles
         * zeropage        STX oper         86    2      3
         * zeropage,Y      STX oper,Y       96    2      4
         * absolute        STX oper         8E    3      4
         */
        case 0x86: {
            bus_write(addr_zpg(cpu), cpu->x);
            cpu->cycles += 3;
            break; 
        }
        case 0x96: {
            bus_write(addr_zpy(cpu), cpu->x);
            cpu->cycles += 4;
            break; 
        }
        case 0x8E: {
            bus_write(addr_abs(cpu), cpu->x);
            cpu->cycles += 4;
            break; 
        }
        /*
         * STY - Store Index Y in Memory
         * Y -> M                          N Z C I D V
         *                                 - - - - - -
         * addressing      assembler       opc  bytes  cycles
         * zeropage        STY oper         84    2      3
         * zeropage,X      STY oper,X       94    2      4
         * absolute        STY oper         8C    3      4
         */

        case 0x84: {
            bus_write(addr_zpg(cpu), cpu->y);
            cpu->cycles += 3;
            break; 
        }
        case 0x94: {
            bus_write(addr_zpx(cpu), cpu->y);
            cpu->cycles += 4;
            break; 
        }
        case 0x8C: {
            bus_write(addr_abs(cpu), cpu->y);
            cpu->cycles += 4;
            break; 
        }
        /*
         * LDX - Load Index X with Memory
         * M -> X                          N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       LDX #oper        A2    2      2
         * zeropage        LDX oper         A6    2      3
         * zeropage,Y      LDX oper,Y       B6    2      4
         * absolute        LDX oper         AE    3      4
         * absolute,Y      LDX oper,Y       BE    3      4*
         */
        case 0xA2:{
            cpu->x = bus_read(addr_imm(cpu));
            cpu->cycles += 2;
            set_zn(cpu, cpu->x);
            break;
        }
        case 0xA6:{
            cpu->x = bus_read(addr_zpg(cpu));
            cpu->cycles += 3;
            set_zn(cpu, cpu->x);
            break;
        }
        case 0xB6:{
            cpu->x = bus_read(addr_zpy(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->x);
            break;
        }
        case 0xAE:{
            cpu->x = bus_read(addr_abs(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->x);
            break;
        }
        case 0xBE:{
            bool crossed;
            cpu->x = bus_read(addr_aby(cpu, &crossed));
            cpu->cycles += 4;
            if (crossed) { cpu->cycles ++; }
            set_zn(cpu, cpu->x);
            break;
        }
        /*
         * LDY - Load Index Y with Memory
         * M -> Y                          N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       LDY #oper        A0    2      2
         * zeropage        LDY oper         A4    2      3
         * zeropage,X      LDY oper,X       B4    2      4
         * absolute        LDY oper         AC    3      4
         * absolute,X      LDY oper,X       BC    3      4*
         */
        case 0xA0:{
            cpu->y = bus_read(addr_imm(cpu));
            cpu->cycles += 2;
            set_zn(cpu, cpu->y);
            break;
        }
        case 0xA4:{
            cpu->y = bus_read(addr_zpg(cpu));
            cpu->cycles += 3;
            set_zn(cpu, cpu->y);
            break;
        }
        case 0xB4:{
            cpu->y = bus_read(addr_zpx(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->y);
            break;
        }
        case 0xAC:{
            cpu->y = bus_read(addr_abs(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->y);
            break;
        }
        case 0xBC:{
            bool crossed;
            cpu->y = bus_read(addr_abx(cpu, &crossed));
            cpu->cycles += 4;
            if (crossed) { cpu->cycles ++; }
            set_zn(cpu, cpu->y);
            break;
        }
        /*
         * TAX - Transfer Accumulator to Index X
         * A -> X                          N Z C I D V
         *                                 + + - - - -
         * implied         TAX              AA    1      2
         */
        case 0xAA: {
            cpu->x = cpu->a;
            cpu->cycles += 2;
            set_zn(cpu, cpu->x);
            break;
        }
        /*
         * TAY - Transfer Accumulator to Index Y
         * A -> Y                          N Z C I D V
         *                                 + + - - - -
         * implied         TAY              A8    1      2
         */
        case 0xA8: {
            cpu->y = cpu->a;
            cpu->cycles += 2;
            set_zn(cpu, cpu->y);
            break;
        }
        /*
         * TXA - Transfer Index X to Accumulator
         * X -> A                          N Z C I D V
         *                                 + + - - - -
         * implied         TXA              8A    1      2
         */
        case 0x8A: {
            cpu->a = cpu->x;
            cpu->cycles += 2;
            set_zn(cpu, cpu->a);
            break;
        }
        /*
         * TYA - Transfer Index Y to Accumulator
         * Y -> A                          N Z C I D V
         *                                 + + - - - -
         * implied         TYA              98    1      2
         */
        case 0x98: {
            cpu->a = cpu->y;
            cpu->cycles += 2;
            set_zn(cpu, cpu->a);
            break;
        }
        /*
         * TSX - Transfer Stack Pointer to Index X
         * SP -> X                         N Z C I D V
         *                                 + + - - - -
         * implied         TSX              BA    1      2
         */
        case 0xBA: {
            cpu->x = cpu->sp;
            cpu->cycles += 2;
            set_zn(cpu, cpu->x);
            break;
        }
        /*
         * TXS - Transfer Index X to Stack Pointer
         * X -> SP                         N Z C I D V
         *                                 - - - - - -
         * implied         TXS              9A    1      2
         */
        case 0x9A: {
            cpu->sp = cpu->x;
            cpu->cycles += 2;
            break;
        }
        /*
         * PHA - Push Accumulator on Stack
         * A -> stack                      N Z C I D V
         *                                 - - - - - -
         * implied         PHA              48    1      3
         */
        case 0x48: {
            bus_write(STACK_BASE + cpu->sp, cpu->a);
            cpu->cycles += 3;
            cpu->sp--;
            break;
        }
        /*
         * PHP - Push Processor Status on Stack
         * status -> stack                 N Z C I D V
         *                                 - - - - - -
         * implied         PHP              08    1      3
         * https://www.nesdev.org/wiki/Status_flags
         * B flag per nes dev fom PHP is always 1
         * guarantee U flag is also set too
         */
        case 0x08: {
            bus_write(STACK_BASE + cpu->sp, cpu->status | FLAG_B | FLAG_U);
            cpu->cycles += 3;
            cpu->sp--;
            break;
        }
        /*
         * PLA - Pull Accumulator from Stack
         * stack -> A                      N Z C I D V
         *                                 + + - - - -
         * implied         PLA              68    1      4
         */
        case 0x68: {
            //pull back up the stack?
            cpu -> sp++;
            cpu->a = bus_read(STACK_BASE+cpu->sp);
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            break;
        }
        /*
         * PLP - Pull Processor Status from Stack
         * stack -> status                 N Z C I D V
         *                                 from stack
         * implied         PLP              28    1      4
         */
        case 0x28: {
            cpu -> sp++;
            cpu->status = bus_read(STACK_BASE+cpu->sp);
            // clear flag B, force flag U
            cpu->status = (cpu->status & ~FLAG_B ) | FLAG_U;
            cpu->cycles += 4;
            break;
        }
        /*
         * AND - AND Memory with Accumulator
         * A AND M -> A                    N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       AND #oper        29    2      2
         * zeropage        AND oper         25    2      3
         * zeropage,X      AND oper,X       35    2      4
         * absolute        AND oper         2D    3      4
         * absolute,X      AND oper,X       3D    3      4*
         * absolute,Y      AND oper,Y       39    3      4*
         * (indirect,X)    AND (oper,X)     21    2      6
         * (indirect),Y    AND (oper),Y     31    2      5*
         */
        case 0x29: {
            cpu -> a = (cpu->a & bus_read(addr_imm(cpu)));
            cpu->cycles += 2;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x25: {
            cpu -> a = (cpu->a & bus_read(addr_zpg(cpu)));
            cpu->cycles += 3;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x35: {
            cpu -> a = (cpu->a & bus_read(addr_zpx(cpu)));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x2D: {
            cpu -> a = (cpu->a & bus_read(addr_abs(cpu)));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x3D: {
            bool crossed;
            cpu -> a = (cpu->a & bus_read(addr_abx(cpu, &crossed)));
            cpu->cycles += 4;
            if(crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x39: {
            bool crossed;
            cpu -> a = (cpu->a & bus_read(addr_aby(cpu, &crossed)));
            cpu->cycles += 4;
            if(crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x21: {
            cpu -> a = (cpu->a & bus_read(addr_izx(cpu)));
            cpu->cycles += 6;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x31: {
            bool crossed;
            cpu -> a = (cpu->a & bus_read(addr_izy(cpu, &crossed)));
            cpu->cycles += 5;
            if(crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }
        
        /*
         * EOR - Exclusive-OR Memory with Accumulator
         * A EOR M -> A                    N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       EOR #oper        49    2      2
         * zeropage        EOR oper         45    2      3
         * zeropage,X      EOR oper,X       55    2      4
         * absolute        EOR oper         4D    3      4
         * absolute,X      EOR oper,X       5D    3      4*
         * absolute,Y      EOR oper,Y       59    3      4*
         * (indirect,X)    EOR (oper,X)     41    2      6
         * (indirect),Y    EOR (oper),Y     51    2      5*
         */
        case 0x49: {
            cpu->a ^= bus_read(addr_imm(cpu));
            cpu->cycles += 2;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x45: {
            cpu->a ^= bus_read(addr_zpg(cpu));
            cpu->cycles += 3;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x55: {
            cpu->a ^= bus_read(addr_zpx(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x4D: {
            cpu->a ^= bus_read(addr_abs(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x5D: {
            bool crossed;
            cpu->a ^= bus_read(addr_abx(cpu, &crossed));
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x59: {
            bool crossed;
            cpu->a ^= bus_read(addr_aby(cpu, &crossed));
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x41: {
            cpu->a ^= bus_read(addr_izx(cpu));
            cpu->cycles += 6;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x51: {
            bool crossed;
            cpu->a ^= bus_read(addr_izy(cpu, &crossed));
            cpu->cycles += 5;
            if (crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }

        /*
         * ORA - OR Memory with Accumulator
         * A OR M -> A                     N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       ORA #oper        09    2      2
         * zeropage        ORA oper         05    2      3
         * zeropage,X      ORA oper,X       15    2      4
         * absolute        ORA oper         0D    3      4
         * absolute,X      ORA oper,X       1D    3      4*
         * absolute,Y      ORA oper,Y       19    3      4*
         * (indirect,X)    ORA (oper,X)     01    2      6
         * (indirect),Y    ORA (oper),Y     11    2      5*
         */
        case 0x09: {
            cpu->a |= bus_read(addr_imm(cpu));
            cpu->cycles += 2;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x05: {
            cpu->a |= bus_read(addr_zpg(cpu));
            cpu->cycles += 3;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x15: {
            cpu->a |= bus_read(addr_zpx(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x0D: {
            cpu->a |= bus_read(addr_abs(cpu));
            cpu->cycles += 4;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x1D: {
            bool crossed;
            cpu->a |= bus_read(addr_abx(cpu, &crossed));
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x19: {
            bool crossed;
            cpu->a |= bus_read(addr_aby(cpu, &crossed));
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x01: {
            cpu->a |= bus_read(addr_izx(cpu));
            cpu->cycles += 6;
            set_zn(cpu, cpu->a);
            break;
        }
        case 0x11: {
            bool crossed;
            cpu->a |= bus_read(addr_izy(cpu, &crossed));
            cpu->cycles += 5;
            if (crossed) cpu->cycles++;
            set_zn(cpu, cpu->a);
            break;
        }

        /*
         * BIT - Test Bits in Memory with Accumulator
         * A AND M, M7 -> N, M6 -> V      N Z C I D V
         *                                 M7+ - - - M6
         * addressing      assembler       opc  bytes  cycles
         * zeropage        BIT oper         24    2      3
         * absolute        BIT oper         2C    3      4
         * 
         * From the site and what I understand..
         * check A & M to see if the result is 0.
         * if result is 0, set Z to 1
         * Also, 
         * check if 7th bit of M is set. if it is, copy it
         * check if 6th bit of M is set. if it is, copy it
         */
        case 0x24: {
            uint8_t val = bus_read(addr_zpg(cpu));
            if (val & FLAG_N) cpu->status |= FLAG_N; else cpu->status &= ~FLAG_N;
            if (val & FLAG_V) cpu->status |= FLAG_V; else cpu->status &= ~FLAG_V;
            if ((cpu->a & val) == 0) cpu->status |= FLAG_Z; else cpu->status &= ~FLAG_Z;
            cpu->cycles += 3;
            break;
        }
        case 0x2C: {
            uint8_t val = bus_read(addr_abs(cpu));
            if (val & FLAG_N) cpu->status |= FLAG_N; else cpu->status &= ~FLAG_N;
            if (val & FLAG_V) cpu->status |= FLAG_V; else cpu->status &= ~FLAG_V;
            if ((cpu->a & val) == 0) cpu->status |= FLAG_Z; else cpu->status &= ~FLAG_Z;
            cpu->cycles += 4;
            break;
        } 
        /*
         * ADC - Add Memory to Accumulator with Carry
         * A + M + C -> A, C               N Z C I D V
         *                                 + + + - - +
         * addressing      assembler       opc  bytes  cycles
         * immediate       ADC #oper        69    2      2
         * zeropage        ADC oper         65    2      3
         * zeropage,X      ADC oper,X       75    2      4
         * absolute        ADC oper         6D    3      4
         * absolute,X      ADC oper,X       7D    3      4*
         * absolute,Y      ADC oper,Y       79    3      4*
         * (indirect,X)    ADC (oper,X)     61    2      6
         * (indirect),Y    ADC (oper),Y     71    2      5*
         */

        /*
         * SBC - Subtract Memory from Accumulator with Borrow
         * A - M - ~C -> A                 N Z C I D V
         *                                 + + + - - +
         * addressing      assembler       opc  bytes  cycles
         * immediate       SBC #oper        E9    2      2
         * zeropage        SBC oper         E5    2      3
         * zeropage,X      SBC oper,X       F5    2      4
         * absolute        SBC oper         ED    3      4
         * absolute,X      SBC oper,X       FD    3      4*
         * absolute,Y      SBC oper,Y       F9    3      4*
         * (indirect,X)    SBC (oper,X)     E1    2      6
         * (indirect),Y    SBC (oper),Y     F1    2      5*
         */

        /*
         * CMP - Compare Memory with Accumulator
         * A - M                           N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       CMP #oper        C9    2      2
         * zeropage        CMP oper         C5    2      3
         * zeropage,X      CMP oper,X       D5    2      4
         * absolute        CMP oper         CD    3      4
         * absolute,X      CMP oper,X       DD    3      4*
         * absolute,Y      CMP oper,Y       D9    3      4*
         * (indirect,X)    CMP (oper,X)     C1    2      6
         * (indirect),Y    CMP (oper),Y     D1    2      5*
         */

        /*
         * CPX - Compare Memory and Index X
         * X - M                           N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       CPX #oper        E0    2      2
         * zeropage        CPX oper         E4    2      3
         * absolute        CPX oper         EC    3      4
         */

        /*
         * CPY - Compare Memory and Index Y
         * Y - M                           N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       CPY #oper        C0    2      2
         * zeropage        CPY oper         C4    2      3
         * absolute        CPY oper         CC    3      4
         */

        /*
         * INC - Increment Memory by One
         * M + 1 -> M                      N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * zeropage        INC oper         E6    2      5
         * zeropage,X      INC oper,X       F6    2      6
         * absolute        INC oper         EE    3      6
         * absolute,X      INC oper,X       FE    3      7
         */

        /*
         * DEC - Decrement Memory by One
         * M - 1 -> M                      N Z C I D V
         *                                 + + - - - -
         * addressing      assembler       opc  bytes  cycles
         * zeropage        DEC oper         C6    2      5
         * zeropage,X      DEC oper,X       D6    2      6
         * absolute        DEC oper         CE    3      6
         * absolute,X      DEC oper,X       DE    3      7
         */

        /*
         * INX - Increment Index X by One
         * X + 1 -> X                      N Z C I D V
         *                                 + + - - - -
         * implied         INX              E8    1      2
         */

        /*
         * INY - Increment Index Y by One
         * Y + 1 -> Y                      N Z C I D V
         *                                 + + - - - -
         * implied         INY              C8    1      2
         */

        /*
         * DEX - Decrement Index X by One
         * X - 1 -> X                      N Z C I D V
         *                                 + + - - - -
         * implied         DEX              CA    1      2
         */

        /*
         * DEY - Decrement Index Y by One
         * Y - 1 -> Y                      N Z C I D V
         *                                 + + - - - -
         * implied         DEY              88    1      2
         */

        /*
         * ASL - Shift Left One Bit (Memory or Accumulator)
         * C <- [76543210] <- 0            N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * accumulator     ASL A            0A    1      2
         * zeropage        ASL oper         06    2      5
         * zeropage,X      ASL oper,X       16    2      6
         * absolute        ASL oper         0E    3      6
         * absolute,X      ASL oper,X       1E    3      7
         */

        /*
         * LSR - Shift One Bit Right (Memory or Accumulator)
         * 0 -> [76543210] -> C            N Z C I D V
         *                                 0 + + - - -
         * addressing      assembler       opc  bytes  cycles
         * accumulator     LSR A            4A    1      2
         * zeropage        LSR oper         46    2      5
         * zeropage,X      LSR oper,X       56    2      6
         * absolute        LSR oper         4E    3      6
         * absolute,X      LSR oper,X       5E    3      7
         */

        /*
         * ROL - Rotate One Bit Left (Memory or Accumulator)
         * C <- [76543210] <- C            N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * accumulator     ROL A            2A    1      2
         * zeropage        ROL oper         26    2      5
         * zeropage,X      ROL oper,X       36    2      6
         * absolute        ROL oper         2E    3      6
         * absolute,X      ROL oper,X       3E    3      7
         */

        /*
         * ROR - Rotate One Bit Right (Memory or Accumulator)
         * C -> [76543210] -> C            N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * accumulator     ROR A            6A    1      2
         * zeropage        ROR oper         66    2      5
         * zeropage,X      ROR oper,X       76    2      6
         * absolute        ROR oper         6E    3      6
         * absolute,X      ROR oper,X       7E    3      7
         */

        /*
         * JMP - Jump to New Location
         * operand -> PC                   N Z C I D V
         *                                 - - - - - -
         * addressing      assembler       opc  bytes  cycles
         * absolute        JMP oper         4C    3      3
         * indirect        JMP (oper)       6C    3      5
         */

        /*
         * JSR - Jump to New Location Saving Return Address
         * push (PC+2), operand -> PC      N Z C I D V
         *                                 - - - - - -
         * absolute        JSR oper         20    3      6
         */

        /*
         * RTS - Return from Subroutine
         * pull PC, PC+1 -> PC             N Z C I D V
         *                                 - - - - - -
         * implied         RTS              60    1      6
         */

        /*
         * RTI - Return from Interrupt
         * pull SR, pull PC                N Z C I D V
         *                                 from stack
         * implied         RTI              40    1      6
         */

        /*
         * BCC - Branch on Carry Clear
         * branch if C = 0                 N Z C I D V
         *                                 - - - - - -
         * relative        BCC oper         90    2      2**
         */

        /*
         * BCS - Branch on Carry Set
         * branch if C = 1                 N Z C I D V
         *                                 - - - - - -
         * relative        BCS oper         B0    2      2**
         */

        /*
         * BEQ - Branch on Result Zero
         * branch if Z = 1                 N Z C I D V
         *                                 - - - - - -
         * relative        BEQ oper         F0    2      2**
         */

        /*
         * BNE - Branch on Result not Zero
         * branch if Z = 0                 N Z C I D V
         *                                 - - - - - -
         * relative        BNE oper         D0    2      2**
         */

        /*
         * BMI - Branch on Result Minus
         * branch if N = 1                 N Z C I D V
         *                                 - - - - - -
         * relative        BMI oper         30    2      2**
         */

        /*
         * BPL - Branch on Result Plus
         * branch if N = 0                 N Z C I D V
         *                                 - - - - - -
         * relative        BPL oper         10    2      2**
         */

        /*
         * BVS - Branch on Overflow Set
         * branch if V = 1                 N Z C I D V
         *                                 - - - - - -
         * relative        BVS oper         70    2      2**
         */

        /*
         * BVC - Branch on Overflow Clear
         * branch if V = 0                 N Z C I D V
         *                                 - - - - - -
         * relative        BVC oper         50    2      2**
         */

        /*
         * CLC - Clear Carry Flag
         * 0 -> C                          N Z C I D V
         *                                 - - 0 - - -
         * implied         CLC              18    1      2
         */

        /*
         * SEC - Set Carry Flag
         * 1 -> C                          N Z C I D V
         *                                 - - 1 - - -
         * implied         SEC              38    1      2
         */

        /*
         * CLD - Clear Decimal Mode
         * 0 -> D                          N Z C I D V
         *                                 - - - - 0 -
         * implied         CLD              D8    1      2
         */

        /*
         * SED - Set Decimal Flag
         * 1 -> D                          N Z C I D V
         *                                 - - - - 1 -
         * implied         SED              F8    1      2
         */

        /*
         * CLI - Clear Interrupt Disable Bit
         * 0 -> I                          N Z C I D V
         *                                 - - - 0 - -
         * implied         CLI              58    1      2
         */

        /*
         * SEI - Set Interrupt Disable Status
         * 1 -> I                          N Z C I D V
         *                                 - - - 1 - -
         * implied         SEI              78    1      2
         */

        /*
         * CLV - Clear Overflow Flag
         * 0 -> V                          N Z C I D V
         *                                 - - - - - 0
         * implied         CLV              B8    1      2
         */

        /*
         * NOP - No Operation
         *                                 N Z C I D V
         *                                 - - - - - -
         * implied         NOP              EA    1      2
         */

        /*
         * BRK - Force Break
         * push PC+2, push SR              N Z C I D V
         *                                 - - - 1 - -
         * implied         BRK              00    1      7
         */

    default:
        fprintf(stderr, "Unknown opcode: %02X at %04X\n", opcode, cpu->pc - 1);
        break;
    }
}
