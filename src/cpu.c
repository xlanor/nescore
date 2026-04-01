#include "cpu.h"
#include "bus.h"
#include <stdbool.h>
#include <stdio.h>

static void set_flag(CPU *cpu, uint8_t flag, bool cond) {
    if (cond) cpu->status |= flag; else cpu->status &= ~flag;
}
static void set_zn(CPU *cpu, uint8_t val) {
    set_flag(cpu, FLAG_Z, val == 0);
    set_flag(cpu, FLAG_N, val & BIT7);
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


void cpu_trace(CPU *cpu) {
    // simplified ver to match nestest.log as we dont have PPU yet... 
    printf("%04X  A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%llu\n",
           cpu->pc, cpu->a, cpu->x, cpu->y, cpu->status, cpu->sp,
           (unsigned long long)cpu->cycles);
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
            cpu->cycles += 5;
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
            set_flag(cpu, FLAG_N, val & FLAG_N);
            set_flag(cpu, FLAG_V, val & FLAG_V);
            set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
            cpu->cycles += 3;
            break;
        }
        case 0x2C: {
            uint8_t val = bus_read(addr_abs(cpu));
            set_flag(cpu, FLAG_N, val & FLAG_N);
            set_flag(cpu, FLAG_V, val & FLAG_V);
            set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
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
        case 0x69: {
            uint8_t val = bus_read(addr_imm(cpu));
            // store in 16 bits first
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            // set C+ if unsigned overflow (0-255)
            set_flag(cpu, FLAG_C, sum > 0xFF);
            // set V+ if signed overflow(-128 to 127)
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            // chop to 8 bits
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 2;
            break;
        }
        // ADC - remaining addressing modes (same logic as 0x69)
        case 0x65: {
            uint8_t val = bus_read(addr_zpg(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 3;
            break;
        }
        case 0x75: {
            uint8_t val = bus_read(addr_zpx(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            break;
        }
        case 0x6D: {
            uint8_t val = bus_read(addr_abs(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            break;
        }
        case 0x7D: {
            bool crossed;
            uint8_t val = bus_read(addr_abx(cpu, &crossed));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            break;
        }
        case 0x79: {
            bool crossed;
            uint8_t val = bus_read(addr_aby(cpu, &crossed));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            break;
        }
        case 0x61: {
            uint8_t val = bus_read(addr_izx(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 6;
            break;
        }
        case 0x71: {
            bool crossed;
            uint8_t val = bus_read(addr_izy(cpu, &crossed));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 5;
            if (crossed) cpu->cycles++;
            break;
        }
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
        // SBC is A + ~M + C (same as ADC with inverted value)
        case 0xE9: {
            uint8_t val = ~bus_read(addr_imm(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 2;
            break;
        }
        case 0xE5: {
            uint8_t val = ~bus_read(addr_zpg(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 3;
            break;
        }
        case 0xF5: {
            uint8_t val = ~bus_read(addr_zpx(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            break;
        }
        case 0xED: {
            uint8_t val = ~bus_read(addr_abs(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            break;
        }
        case 0xFD: {
            bool crossed;
            uint8_t val = ~bus_read(addr_abx(cpu, &crossed));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            break;
        }
        case 0xF9: {
            bool crossed;
            uint8_t val = ~bus_read(addr_aby(cpu, &crossed));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            break;
        }
        case 0xE1: {
            uint8_t val = ~bus_read(addr_izx(cpu));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 6;
            break;
        }
        case 0xF1: {
            bool crossed;
            uint8_t val = ~bus_read(addr_izy(cpu, &crossed));
            uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
            set_flag(cpu, FLAG_C, sum > 0xFF);
            set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
            cpu->a = sum & 0xFF;
            set_zn(cpu, cpu->a);
            cpu->cycles += 5;
            if (crossed) cpu->cycles++;
            break;
        }

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
        // CMP - all addressing modes
        case 0xC9: {
            uint8_t val = bus_read(addr_imm(cpu));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 2;
            break;
        }
        case 0xC5: {
            uint8_t val = bus_read(addr_zpg(cpu));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 3;
            break;
        }
        case 0xD5: {
            uint8_t val = bus_read(addr_zpx(cpu));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 4;
            break;
        }
        case 0xCD: {
            uint8_t val = bus_read(addr_abs(cpu));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 4;
            break;
        }
        case 0xDD: {
            bool crossed;
            uint8_t val = bus_read(addr_abx(cpu, &crossed));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            break;
        }
        case 0xD9: {
            bool crossed;
            uint8_t val = bus_read(addr_aby(cpu, &crossed));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 4;
            if (crossed) cpu->cycles++;
            break;
        }
        case 0xC1: {
            uint8_t val = bus_read(addr_izx(cpu));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 6;
            break;
        }
        case 0xD1: {
            bool crossed;
            uint8_t val = bus_read(addr_izy(cpu, &crossed));
            set_flag(cpu, FLAG_C, cpu->a >= val);
            set_zn(cpu, (cpu->a - val) & 0xFF);
            cpu->cycles += 5;
            if (crossed) cpu->cycles++;
            break;
        }
        /*
         * CPX - Compare Memory and Index X
         * X - M                           N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       CPX #oper        E0    2      2
         * zeropage        CPX oper         E4    2      3
         * absolute        CPX oper         EC    3      4
         */
        case 0xE0: {
            uint8_t val = bus_read(addr_imm(cpu));
            set_flag(cpu, FLAG_C, cpu->x >= val);
            set_zn(cpu, (cpu->x - val) & 0xFF);
            cpu->cycles += 2;
            break;
        }
        case 0xE4: {
            uint8_t val = bus_read(addr_zpg(cpu));
            set_flag(cpu, FLAG_C, cpu->x >= val);
            set_zn(cpu, (cpu->x - val) & 0xFF);
            cpu->cycles += 3;
            break;
        }
        case 0xEC: {
            uint8_t val = bus_read(addr_abs(cpu));
            set_flag(cpu, FLAG_C, cpu->x >= val);
            set_zn(cpu, (cpu->x - val) & 0xFF);
            cpu->cycles += 4;
            break;
        }
        /*
         * CPY - Compare Memory and Index Y
         * Y - M                           N Z C I D V
         *                                 + + + - - -
         * addressing      assembler       opc  bytes  cycles
         * immediate       CPY #oper        C0    2      2
         * zeropage        CPY oper         C4    2      3
         * absolute        CPY oper         CC    3      4
         */
        case 0xC0: {
            uint8_t val = bus_read(addr_imm(cpu));
            set_flag(cpu, FLAG_C, cpu->y >= val);
            set_zn(cpu, (cpu->y - val) & 0xFF);
            cpu->cycles += 2;
            break;
        }
        case 0xC4: {
            uint8_t val = bus_read(addr_zpg(cpu));
            set_flag(cpu, FLAG_C, cpu->y >= val);
            set_zn(cpu, (cpu->y - val) & 0xFF);
            cpu->cycles += 3;
            break;
        }
        case 0xCC: {
            uint8_t val = bus_read(addr_abs(cpu));
            set_flag(cpu, FLAG_C, cpu->y >= val);
            set_zn(cpu, (cpu->y - val) & 0xFF);
            cpu->cycles += 4;
            break;
        }

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
        case 0xE6: {
            uint16_t addr = addr_zpg(cpu);
            uint8_t val = bus_read(addr) + 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 5;
            break;
        }
        case 0xF6: {
            uint16_t addr = addr_zpx(cpu);
            uint8_t val = bus_read(addr) + 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0xEE: {
            uint16_t addr = addr_abs(cpu);
            uint8_t val = bus_read(addr) + 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0xFE: {
            uint16_t addr = addr_abx(cpu, NULL);
            uint8_t val = bus_read(addr) + 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 7;
            break;
        }

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
        case 0xC6: {
            uint16_t addr = addr_zpg(cpu);
            uint8_t val = bus_read(addr) - 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 5;
            break;
        }
        case 0xD6: {
            uint16_t addr = addr_zpx(cpu);
            uint8_t val = bus_read(addr) - 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0xCE: {
            uint16_t addr = addr_abs(cpu);
            uint8_t val = bus_read(addr) - 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0xDE: {
            uint16_t addr = addr_abx(cpu, NULL);
            uint8_t val = bus_read(addr) - 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 7;
            break;
        }

        /*
         * INX - Increment Index X by One
         * X + 1 -> X                      N Z C I D V
         *                                 + + - - - -
         * implied         INX              E8    1      2
         */
        case 0xE8: {
            cpu->x++;
            set_zn(cpu, cpu->x);
            cpu->cycles += 2;
            break;
        }

        /*
         * INY - Increment Index Y by One
         * Y + 1 -> Y                      N Z C I D V
         *                                 + + - - - -
         * implied         INY              C8    1      2
         */
        case 0xC8: {
            cpu->y++;
            set_zn(cpu, cpu->y);
            cpu->cycles += 2;
            break;
        }

        /*
         * DEX - Decrement Index X by One
         * X - 1 -> X                      N Z C I D V
         *                                 + + - - - -
         * implied         DEX              CA    1      2
         */
        case 0xCA: {
            cpu->x--;
            set_zn(cpu, cpu->x);
            cpu->cycles += 2;
            break;
        }

        /*
         * DEY - Decrement Index Y by One
         * Y - 1 -> Y                      N Z C I D V
         *                                 + + - - - -
         * implied         DEY              88    1      2
         */
        case 0x88: {
            cpu->y--;
            set_zn(cpu, cpu->y);
            cpu->cycles += 2;
            break;
        }

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
        // ASL accumulator
        case 0x0A: {
            set_flag(cpu, FLAG_C, cpu->a & BIT7);
            cpu->a <<= 1;
            set_zn(cpu, cpu->a);
            cpu->cycles += 2;
            break;
        }
        // ASL memory
        case 0x06: {
            uint16_t addr = addr_zpg(cpu);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT7);
            val <<= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 5;
            break;
        }
        case 0x16: {
            uint16_t addr = addr_zpx(cpu);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT7);
            val <<= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x0E: {
            uint16_t addr = addr_abs(cpu);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT7);
            val <<= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x1E: {
            uint16_t addr = addr_abx(cpu, NULL);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT7);
            val <<= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 7;
            break;
        }

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
        // LSR accumulator
        case 0x4A: {
            set_flag(cpu, FLAG_C, cpu->a & BIT0);
            cpu->a >>= 1;
            set_zn(cpu, cpu->a);
            cpu->cycles += 2;
            break;
        }
        // LSR memory
        case 0x46: {
            uint16_t addr = addr_zpg(cpu);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT0);
            val >>= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 5;
            break;
        }
        case 0x56: {
            uint16_t addr = addr_zpx(cpu);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT0);
            val >>= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x4E: {
            uint16_t addr = addr_abs(cpu);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT0);
            val >>= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x5E: {
            uint16_t addr = addr_abx(cpu, NULL);
            uint8_t val = bus_read(addr);
            set_flag(cpu, FLAG_C, val & BIT0);
            val >>= 1;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 7;
            break;
        }

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
        // ROL accumulator
        case 0x2A: {
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, cpu->a & BIT7);
            cpu->a = (cpu->a << 1) | old_carry;
            set_zn(cpu, cpu->a);
            cpu->cycles += 2;
            break;
        }
        // ROL memory
        case 0x26: {
            uint16_t addr = addr_zpg(cpu);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT7);
            val = (val << 1) | old_carry;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 5;
            break;
        }
        case 0x36: {
            uint16_t addr = addr_zpx(cpu);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT7);
            val = (val << 1) | old_carry;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x2E: {
            uint16_t addr = addr_abs(cpu);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT7);
            val = (val << 1) | old_carry;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x3E: {
            uint16_t addr = addr_abx(cpu, NULL);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT7);
            val = (val << 1) | old_carry;
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 7;
            break;
        }

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
        // ROR accumulator
        case 0x6A: {
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, cpu->a & BIT0);
            cpu->a = (cpu->a >> 1) | (old_carry ? BIT7 : 0);
            set_zn(cpu, cpu->a);
            cpu->cycles += 2;
            break;
        }
        // ROR memory
        case 0x66: {
            uint16_t addr = addr_zpg(cpu);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT0);
            val = (val >> 1) | (old_carry ? BIT7 : 0);
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 5;
            break;
        }
        case 0x76: {
            uint16_t addr = addr_zpx(cpu);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT0);
            val = (val >> 1) | (old_carry ? BIT7 : 0);
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x6E: {
            uint16_t addr = addr_abs(cpu);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT0);
            val = (val >> 1) | (old_carry ? BIT7 : 0);
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 6;
            break;
        }
        case 0x7E: {
            uint16_t addr = addr_abx(cpu, NULL);
            uint8_t val = bus_read(addr);
            bool old_carry = cpu->status & FLAG_C;
            set_flag(cpu, FLAG_C, val & BIT0);
            val = (val >> 1) | (old_carry ? BIT7 : 0);
            bus_write(addr, val);
            set_zn(cpu, val);
            cpu->cycles += 7;
            break;
        }

        /*
         * JMP - Jump to New Location
         * operand -> PC                   N Z C I D V
         *                                 - - - - - -
         * addressing      assembler       opc  bytes  cycles
         * absolute        JMP oper         4C    3      3
         * indirect        JMP (oper)       6C    3      5
         */
        case 0x4C: {
            cpu->pc = addr_abs(cpu);
            cpu->cycles += 3;
            break;
        }
        case 0x6C: {
            // JMP indirect has a hardware bug on the 6502:
            // if the pointer address falls on a page boundary ($xxFF),
            // the high byte wraps within the page instead of crossing.
            // e.g. JMP ($10FF) reads low from $10FF and high from $1000,
            // NOT $1100. See: https://www.nesdev.org/wiki/Errata
            uint16_t ptr = read16(cpu);
            uint16_t addr;
            if ((ptr & 0xFF) == 0xFF) {
                addr = bus_read(ptr) | (bus_read(ptr & 0xFF00) << 8);
            } else {
                addr = bus_read(ptr) | (bus_read(ptr + 1) << 8);
            }
            cpu->pc = addr;
            cpu->cycles += 5;
            break;
        }

        /*
         * JSR - Jump to New Location Saving Return Address
         * push (PC+2), operand -> PC      N Z C I D V
         *                                 - - - - - -
         * absolute        JSR oper         20    3      6
         */
        case 0x20: {
            // push PC+2 (address of last byte of JSR instruction)
            // PC is already pointing at the low byte of the address,
            // so PC+1 is the return address - 1 (RTS will add 1)
            uint16_t ret = cpu->pc + 1;
            bus_write(STACK_BASE + cpu->sp, (ret >> 8) & 0xFF);
            cpu->sp--;
            bus_write(STACK_BASE + cpu->sp, ret & 0xFF);
            cpu->sp--;
            cpu->pc = addr_abs(cpu);
            cpu->cycles += 6;
            break;
        }

        /*
         * RTS - Return from Subroutine
         * pull PC, PC+1 -> PC             N Z C I D V
         *                                 - - - - - -
         * implied         RTS              60    1      6
         */
        case 0x60: {
            cpu->sp++;
            uint16_t lo = bus_read(STACK_BASE + cpu->sp);
            cpu->sp++;
            uint16_t hi = bus_read(STACK_BASE + cpu->sp);
            cpu->pc = ((hi << 8) | lo) + 1;
            cpu->cycles += 6;
            break;
        }

        /*
         * RTI - Return from Interrupt
         * pull SR, pull PC                N Z C I D V
         *                                 from stack
         * implied         RTI              40    1      6
         */
        case 0x40: {
            // pull status (same masking as PLP)
            cpu->sp++;
            cpu->status = (bus_read(STACK_BASE + cpu->sp) & ~FLAG_B) | FLAG_U;
            // pull PC (no +1, unlike RTS)
            cpu->sp++;
            uint16_t lo = bus_read(STACK_BASE + cpu->sp);
            cpu->sp++;
            uint16_t hi = bus_read(STACK_BASE + cpu->sp);
            cpu->pc = (hi << 8) | lo;
            cpu->cycles += 6;
            break;
        }

        /*
         * Branches - all use relative addressing
         * 2** = +1 if branch taken, +2 if page crossed
         */
        // BCC - Branch on Carry Clear
        case 0x90: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (!(cpu->status & FLAG_C)) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }
        // BCS - Branch on Carry Set
        case 0xB0: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (cpu->status & FLAG_C) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }
        // BEQ - Branch on Result Zero
        case 0xF0: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (cpu->status & FLAG_Z) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }
        // BNE - Branch on Result not Zero
        case 0xD0: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (!(cpu->status & FLAG_Z)) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }
        // BMI - Branch on Result Minus
        case 0x30: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (cpu->status & FLAG_N) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }
        // BPL - Branch on Result Plus
        case 0x10: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (!(cpu->status & FLAG_N)) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }
        // BVS - Branch on Overflow Set
        case 0x70: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (cpu->status & FLAG_V) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }
        // BVC - Branch on Overflow Clear
        case 0x50: {
            int8_t offset = (int8_t)bus_read(cpu->pc++);
            cpu->cycles += 2;
            if (!(cpu->status & FLAG_V)) {
                uint16_t old_pc = cpu->pc;
                cpu->pc += offset;
                cpu->cycles++;
                if (page_crossed(old_pc, cpu->pc)) cpu->cycles++;
            }
            break;
        }

        // Flag instructions
        // CLC - Clear Carry Flag
        case 0x18: {
            cpu->status &= ~FLAG_C;
            cpu->cycles += 2;
            break;
        }
        // SEC - Set Carry Flag
        case 0x38: {
            cpu->status |= FLAG_C;
            cpu->cycles += 2;
            break;
        }
        // CLD - Clear Decimal Mode
        case 0xD8: {
            cpu->status &= ~FLAG_D;
            cpu->cycles += 2;
            break;
        }
        // SED - Set Decimal Flag
        case 0xF8: {
            cpu->status |= FLAG_D;
            cpu->cycles += 2;
            break;
        }
        // CLI - Clear Interrupt Disable Bit
        case 0x58: {
            cpu->status &= ~FLAG_I;
            cpu->cycles += 2;
            break;
        }
        // SEI - Set Interrupt Disable Status
        case 0x78: {
            cpu->status |= FLAG_I;
            cpu->cycles += 2;
            break;
        }
        // CLV - Clear Overflow Flag
        case 0xB8: {
            cpu->status &= ~FLAG_V;
            cpu->cycles += 2;
            break;
        }

        // NOP - No Operation
        case 0xEA: {
            cpu->cycles += 2;
            break;
        }

        // BRK - Force Break
        case 0x00: {
            cpu->pc++; // BRK skips the next byte (padding)
            bus_write(STACK_BASE + cpu->sp, (cpu->pc >> 8) & 0xFF);
            cpu->sp--;
            bus_write(STACK_BASE + cpu->sp, cpu->pc & 0xFF);
            cpu->sp--;
            bus_write(STACK_BASE + cpu->sp, cpu->status | FLAG_B | FLAG_U);
            cpu->sp--;
            cpu->status |= FLAG_I;
            cpu->pc = bus_read(0xFFFE) | (bus_read(0xFFFF) << 8);
            cpu->cycles += 7;
            break;
        }

    default:
        fprintf(stderr, "Unknown opcode: %02X at %04X\n", opcode, cpu->pc - 1);
        break;
    }
}
