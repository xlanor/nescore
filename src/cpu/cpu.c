#include "cpu.h"
#include "cpu_ops.h"
#include "../bus/bus.h"
#include <stdbool.h>
#include <stdio.h>

static bool page_crossed(uint16_t a, uint16_t b) {
    return (a & 0xFF00) != (b & 0xFF00);
}

void cpu_init(CPU *cpu) {
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->sp = 0xFD;
    cpu->status = FLAG_U | FLAG_I;
    cpu->pc = 0;
    cpu->cycles = 0;
    cpu->opcode = 0;
    cpu->cycle = 0;
    cpu->addr = 0;
    cpu->data = 0;
}

void cpu_reset(CPU *cpu) {
    uint16_t lo = bus_read(cpu->bus, 0xFFFC);
    uint16_t hi = bus_read(cpu->bus, 0xFFFD);
    cpu->pc = (hi << 8) | lo;
    cpu->sp = 0xFD;
    cpu->status = FLAG_U | FLAG_I;
    cpu->cycles = 7;
    cpu->cycle = 0;
}

void cpu_trace(CPU *cpu) {
    printf("%04X  A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%llu\n",
           cpu->pc, cpu->a, cpu->x, cpu->y, cpu->status, cpu->sp,
           (unsigned long long)cpu->cycles);
}

void cpu_step(CPU *cpu) {
    cpu_tick(cpu);
    while (cpu->cycle != 0) {
        cpu_tick(cpu);
    }
}

void cpu_tick(CPU *cpu) {
    Bus *bus = cpu->bus;

    if (cpu->cycle == 0) {
        cpu->opcode = bus_read(bus, cpu->pc++);
        cpu->cycle = 1;
        cpu->cycles++;
        return;
    }

    const OpcodeEntry *entry = &opcode_table[cpu->opcode];
    CyclePattern pat = entry->pattern;

    switch (pat) {

    case PAT_IMPLIED:
        bus_read(bus, cpu->pc);
        if (entry->execute) entry->execute(cpu);
        cpu->cycle = 0;
        break;

    case PAT_IMMEDIATE:
        cpu->data = bus_read(bus, cpu->pc++);
        if (entry->execute) entry->execute(cpu);
        cpu->cycle = 0;
        break;

    case PAT_ZPG_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ZPG_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ZPG_RMW:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 3;
            break;
        case 3:
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 4;
            break;
        case 4:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ZPX_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->addr);
            cpu->addr = (cpu->addr + cpu->x) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ZPX_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->addr);
            cpu->addr = (cpu->addr + cpu->x) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ZPX_RMW:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->addr);
            cpu->addr = (cpu->addr + cpu->x) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 4;
            break;
        case 4:
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 5;
            break;
        case 5:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ZPY_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->addr);
            cpu->addr = (cpu->addr + cpu->y) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ZPY_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->addr);
            cpu->addr = (cpu->addr + cpu->y) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABS_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr |= bus_read(bus, cpu->pc++) << 8;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABS_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr |= bus_read(bus, cpu->pc++) << 8;
            cpu->cycle = 3;
            break;
        case 3:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABS_RMW:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr |= bus_read(bus, cpu->pc++) << 8;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 4;
            break;
        case 4:
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 5;
            break;
        case 5:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABX_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2: {
            uint8_t hi = bus_read(bus, cpu->pc++);
            uint16_t base = (hi << 8) | cpu->addr;
            cpu->addr = base + cpu->x;
            cpu->data = page_crossed(base, cpu->addr) ? 1 : 0;
            cpu->cycle = 3;
            break;
        }
        case 3:
            if (cpu->data) {
                bus_read(bus, (cpu->addr - 0x100));
                cpu->cycle = 4;
            } else {
                cpu->data = bus_read(bus, cpu->addr);
                entry->execute(cpu);
                cpu->cycle = 0;
            }
            break;
        case 4:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABX_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2: {
            uint8_t hi = bus_read(bus, cpu->pc++);
            cpu->addr = ((hi << 8) | cpu->addr) + cpu->x;
            cpu->cycle = 3;
            break;
        }
        case 3:
            bus_read(bus, cpu->addr);
            cpu->cycle = 4;
            break;
        case 4:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABX_RMW:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2: {
            uint8_t hi = bus_read(bus, cpu->pc++);
            cpu->addr = ((hi << 8) | cpu->addr) + cpu->x;
            cpu->cycle = 3;
            break;
        }
        case 3:
            bus_read(bus, cpu->addr);
            cpu->cycle = 4;
            break;
        case 4:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 5;
            break;
        case 5:
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 6;
            break;
        case 6:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABY_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2: {
            uint8_t hi = bus_read(bus, cpu->pc++);
            uint16_t base = (hi << 8) | cpu->addr;
            cpu->addr = base + cpu->y;
            cpu->data = page_crossed(base, cpu->addr) ? 1 : 0;
            cpu->cycle = 3;
            break;
        }
        case 3:
            if (cpu->data) {
                bus_read(bus, (cpu->addr - 0x100));
                cpu->cycle = 4;
            } else {
                cpu->data = bus_read(bus, cpu->addr);
                entry->execute(cpu);
                cpu->cycle = 0;
            }
            break;
        case 4:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABY_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2: {
            uint8_t hi = bus_read(bus, cpu->pc++);
            cpu->addr = ((hi << 8) | cpu->addr) + cpu->y;
            cpu->cycle = 3;
            break;
        }
        case 3:
            bus_read(bus, cpu->addr);
            cpu->cycle = 4;
            break;
        case 4:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_ABY_RMW:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2: {
            uint8_t hi = bus_read(bus, cpu->pc++);
            cpu->addr = ((hi << 8) | cpu->addr) + cpu->y;
            cpu->cycle = 3;
            break;
        }
        case 3:
            bus_read(bus, cpu->addr);
            cpu->cycle = 4;
            break;
        case 4:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 5;
            break;
        case 5:
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 6;
            break;
        case 6:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_IZX_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->data = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->data);
            cpu->data = (cpu->data + cpu->x) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->addr = bus_read(bus, cpu->data);
            cpu->cycle = 4;
            break;
        case 4:
            cpu->addr |= bus_read(bus, (cpu->data + 1) & 0xFF) << 8;
            cpu->cycle = 5;
            break;
        case 5:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_IZX_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->data = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->data);
            cpu->data = (cpu->data + cpu->x) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->addr = bus_read(bus, cpu->data);
            cpu->cycle = 4;
            break;
        case 4:
            cpu->addr |= bus_read(bus, (cpu->data + 1) & 0xFF) << 8;
            cpu->cycle = 5;
            break;
        case 5:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_IZX_RMW:
        switch (cpu->cycle) {
        case 1:
            cpu->data = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, cpu->data);
            cpu->data = (cpu->data + cpu->x) & 0xFF;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->addr = bus_read(bus, cpu->data);
            cpu->cycle = 4;
            break;
        case 4:
            cpu->addr |= bus_read(bus, (cpu->data + 1) & 0xFF) << 8;
            cpu->cycle = 5;
            break;
        case 5:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 6;
            break;
        case 6:
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 7;
            break;
        case 7:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_IZY_READ:
        switch (cpu->cycle) {
        case 1:
            cpu->data = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr = bus_read(bus, cpu->data);
            cpu->cycle = 3;
            break;
        case 3: {
            uint8_t hi = bus_read(bus, (cpu->data + 1) & 0xFF);
            uint16_t base = (hi << 8) | cpu->addr;
            cpu->addr = base + cpu->y;
            cpu->data = page_crossed(base, cpu->addr) ? 1 : 0;
            cpu->cycle = 4;
            break;
        }
        case 4:
            if (cpu->data) {
                bus_read(bus, (cpu->addr - 0x100));
                cpu->cycle = 5;
            } else {
                cpu->data = bus_read(bus, cpu->addr);
                entry->execute(cpu);
                cpu->cycle = 0;
            }
            break;
        case 5:
            cpu->data = bus_read(bus, cpu->addr);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_IZY_WRITE:
        switch (cpu->cycle) {
        case 1:
            cpu->data = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr = bus_read(bus, cpu->data);
            cpu->cycle = 3;
            break;
        case 3: {
            uint8_t hi = bus_read(bus, (cpu->data + 1) & 0xFF);
            cpu->addr = ((hi << 8) | cpu->addr) + cpu->y;
            cpu->cycle = 4;
            break;
        }
        case 4:
            bus_read(bus, cpu->addr);
            cpu->cycle = 5;
            break;
        case 5:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_IZY_RMW:
        switch (cpu->cycle) {
        case 1:
            cpu->data = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr = bus_read(bus, cpu->data);
            cpu->cycle = 3;
            break;
        case 3: {
            uint8_t hi = bus_read(bus, (cpu->data + 1) & 0xFF);
            cpu->addr = ((hi << 8) | cpu->addr) + cpu->y;
            cpu->cycle = 4;
            break;
        }
        case 4:
            bus_read(bus, cpu->addr);
            cpu->cycle = 5;
            break;
        case 5:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 6;
            break;
        case 6:
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 7;
            break;
        case 7:
            entry->execute(cpu);
            bus_write(bus, cpu->addr, cpu->data);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_BRANCH:
        switch (cpu->cycle) {
        case 1: {
            int8_t offset = (int8_t)bus_read(bus, cpu->pc++);
            entry->execute(cpu);
            if (!cpu->data) {
                cpu->cycle = 0;
            } else {
                cpu->addr = cpu->pc + offset;
                cpu->cycle = 2;
            }
            break;
        }
        case 2:
            bus_read(bus, cpu->pc);
            if (page_crossed(cpu->pc, cpu->addr)) {
                cpu->cycle = 3;
            } else {
                cpu->pc = cpu->addr;
                cpu->cycle = 0;
            }
            break;
        case 3:
            bus_read(bus, cpu->pc);
            cpu->pc = cpu->addr;
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_PUSH:
        switch (cpu->cycle) {
        case 1:
            bus_read(bus, cpu->pc);
            cpu->cycle = 2;
            break;
        case 2:
            entry->execute(cpu);
            bus_write(bus, STACK_BASE + cpu->sp, cpu->data);
            cpu->sp--;
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_PULL:
        switch (cpu->cycle) {
        case 1:
            bus_read(bus, cpu->pc);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, STACK_BASE + cpu->sp);
            cpu->sp++;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->data = bus_read(bus, STACK_BASE + cpu->sp);
            entry->execute(cpu);
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_JSR:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, STACK_BASE + cpu->sp);
            cpu->cycle = 3;
            break;
        case 3:
            bus_write(bus, STACK_BASE + cpu->sp, (cpu->pc >> 8) & 0xFF);
            cpu->sp--;
            cpu->cycle = 4;
            break;
        case 4:
            bus_write(bus, STACK_BASE + cpu->sp, cpu->pc & 0xFF);
            cpu->sp--;
            cpu->cycle = 5;
            break;
        case 5:
            cpu->addr |= bus_read(bus, cpu->pc) << 8;
            cpu->pc = cpu->addr;
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_RTS:
        switch (cpu->cycle) {
        case 1:
            bus_read(bus, cpu->pc);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, STACK_BASE + cpu->sp);
            cpu->sp++;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->addr = bus_read(bus, STACK_BASE + cpu->sp);
            cpu->sp++;
            cpu->cycle = 4;
            break;
        case 4:
            cpu->addr |= bus_read(bus, STACK_BASE + cpu->sp) << 8;
            cpu->cycle = 5;
            break;
        case 5:
            bus_read(bus, cpu->addr);
            cpu->pc = cpu->addr + 1;
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_RTI:
        switch (cpu->cycle) {
        case 1:
            bus_read(bus, cpu->pc);
            cpu->cycle = 2;
            break;
        case 2:
            bus_read(bus, STACK_BASE + cpu->sp);
            cpu->sp++;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->status = (bus_read(bus, STACK_BASE + cpu->sp) & ~FLAG_B) | FLAG_U;
            cpu->sp++;
            cpu->cycle = 4;
            break;
        case 4:
            cpu->addr = bus_read(bus, STACK_BASE + cpu->sp);
            cpu->sp++;
            cpu->cycle = 5;
            break;
        case 5:
            cpu->addr |= bus_read(bus, STACK_BASE + cpu->sp) << 8;
            cpu->pc = cpu->addr;
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_BRK:
        switch (cpu->cycle) {
        case 1:
            bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            bus_write(bus, STACK_BASE + cpu->sp, (cpu->pc >> 8) & 0xFF);
            cpu->sp--;
            cpu->cycle = 3;
            break;
        case 3:
            bus_write(bus, STACK_BASE + cpu->sp, cpu->pc & 0xFF);
            cpu->sp--;
            cpu->cycle = 4;
            break;
        case 4:
            bus_write(bus, STACK_BASE + cpu->sp, cpu->status | FLAG_B | FLAG_U);
            cpu->sp--;
            cpu->cycle = 5;
            break;
        case 5:
            cpu->addr = bus_read(bus, 0xFFFE);
            cpu->cycle = 6;
            break;
        case 6:
            cpu->addr |= bus_read(bus, 0xFFFF) << 8;
            cpu->pc = cpu->addr;
            cpu->status |= FLAG_I;
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_JMP_ABS:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr |= bus_read(bus, cpu->pc++) << 8;
            cpu->pc = cpu->addr;
            cpu->cycle = 0;
            break;
        }
        break;

    case PAT_JMP_IND:
        switch (cpu->cycle) {
        case 1:
            cpu->addr = bus_read(bus, cpu->pc++);
            cpu->cycle = 2;
            break;
        case 2:
            cpu->addr |= bus_read(bus, cpu->pc++) << 8;
            cpu->cycle = 3;
            break;
        case 3:
            cpu->data = bus_read(bus, cpu->addr);
            cpu->cycle = 4;
            break;
        case 4: {
            uint16_t hi_addr;
            if ((cpu->addr & 0xFF) == 0xFF) {
                hi_addr = cpu->addr & 0xFF00;
            } else {
                hi_addr = cpu->addr + 1;
            }
            cpu->pc = cpu->data | (bus_read(bus, hi_addr) << 8);
            cpu->cycle = 0;
            break;
        }
        }
        break;

    default:
        fprintf(stderr, "Unknown pattern for opcode: %02X\n", cpu->opcode);
        cpu->cycle = 0;
        break;
    }

    cpu->cycles++;
}
