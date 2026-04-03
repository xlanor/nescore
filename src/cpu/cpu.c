#include "cpu.h"
#include "cpu_ops.h"
#include "bus/bus.h"
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
    cpu->reset_pending    = false;
    cpu->nmi_pending      = false;
    cpu->irq_pending      = false;
    cpu->active_interrupt = 0;
}

// Arms the reset sequence. The 7-cycle hardware sequence runs through
// cpu_tick as PAT_RESET — see https://www.nesdev.org/wiki/CPU_interrupts
void cpu_reset(CPU *cpu) {
    cpu->reset_pending = true;
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
        // Interrupt priority: RESET > NMI > IRQ
        // https://www.nesdev.org/wiki/CPU_interrupts
        if (cpu->reset_pending) {
            cpu->reset_pending    = false;
            cpu->active_interrupt = PAT_RESET;
            cpu->sp               = 0x00;  // cold boot: 0x00 - 3 = 0xFD after phantom stack reads
            cpu->status           = FLAG_U;
            bus_read(bus, cpu->pc);  // cycle 1: dummy read, PC not incremented
            cpu->cycle = 2;
            return;
        }
        if (cpu->nmi_pending) {
            cpu->nmi_pending      = false;
            cpu->active_interrupt = PAT_NMI;
            bus_read(bus, cpu->pc);  // cycle 1: dummy read
            cpu->cycle = 2;
            return;
        }
        if (cpu->irq_pending && !(cpu->status & FLAG_I)) {
            cpu->irq_pending      = false;
            cpu->active_interrupt = PAT_IRQ;
            bus_read(bus, cpu->pc);  // cycle 1: dummy read
            cpu->cycle = 2;
            return;
        }
        cpu->opcode = bus_read(bus, cpu->pc++);
        cpu->cycle = 1;
        return;
    }

    const OpcodeEntry *entry = &opcode_table[cpu->opcode];
    CyclePattern pat = cpu->active_interrupt ? cpu->active_interrupt : entry->pattern;

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

    // RESET: same 7-cycle sequence as IRQ/NMI but writes suppressed (reads) on cycles 3-5
    // https://www.nesdev.org/wiki/CPU_interrupts
    // cycle 1 handled at cycle 0 dispatch (dummy read of PC)
    case PAT_RESET:
        switch (cpu->cycle) {
        case 2:
            bus_read(bus, cpu->pc);          // dummy read, PC not incremented
            cpu->cycle = 3;
            break;
        case 3:
            bus_read(bus, STACK_BASE + cpu->sp);  // phantom read (no write)
            cpu->sp--;
            cpu->cycle = 4;
            break;
        case 4:
            bus_read(bus, STACK_BASE + cpu->sp);  // phantom read (no write)
            cpu->sp--;
            cpu->cycle = 5;
            break;
        case 5:
            bus_read(bus, STACK_BASE + cpu->sp);  // phantom read (no write)
            cpu->sp--;
            cpu->cycle = 6;
            break;
        case 6:
            cpu->addr = bus_read(bus, 0xFFFC);
            cpu->status |= FLAG_I;
            cpu->cycle = 7;
            break;
        case 7:
            cpu->addr |= bus_read(bus, 0xFFFD) << 8;
            cpu->pc = cpu->addr;
            cpu->active_interrupt = 0;
            cpu->cycle = 0;
            break;
        }
        break;

    // NMI: edge-triggered, vector $FFFA/$FFFB, B flag clear
    // cycle 1 handled at cycle 0 dispatch (dummy read of PC)
    case PAT_NMI:
        switch (cpu->cycle) {
        case 2:
            bus_read(bus, cpu->pc);
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
            // NMI hijack check happens here — already PAT_NMI so no change needed
            cpu->cycle = 5;
            break;
        case 5:
            bus_write(bus, STACK_BASE + cpu->sp, cpu->status & ~FLAG_B);
            cpu->sp--;
            cpu->cycle = 6;
            break;
        case 6:
            cpu->addr = bus_read(bus, 0xFFFA);
            cpu->status |= FLAG_I;
            cpu->cycle = 7;
            break;
        case 7:
            cpu->addr |= bus_read(bus, 0xFFFB) << 8;
            cpu->pc = cpu->addr;
            cpu->active_interrupt = 0;
            cpu->cycle = 0;
            break;
        }
        break;

    // IRQ: level-sensitive (checked when I=0), vector $FFFE/$FFFF, B flag clear
    // NMI can hijack at cycle 4 (after PCL push, before P push)
    // cycle 1 handled at cycle 0 dispatch (dummy read of PC)
    case PAT_IRQ:
        switch (cpu->cycle) {
        case 2:
            bus_read(bus, cpu->pc);
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
            // NMI hijack: if NMI asserted, switch to NMI vector
            if (cpu->nmi_pending) {
                cpu->nmi_pending      = false;
                cpu->active_interrupt = PAT_NMI;
            }
            cpu->cycle = 5;
            break;
        case 5:
            bus_write(bus, STACK_BASE + cpu->sp, cpu->status & ~FLAG_B);
            cpu->sp--;
            cpu->cycle = 6;
            break;
        case 6: {
            uint16_t vec_lo = (cpu->active_interrupt == PAT_NMI) ? 0xFFFA : 0xFFFE;
            cpu->addr = bus_read(bus, vec_lo);
            cpu->status |= FLAG_I;
            cpu->cycle = 7;
            break;
        }
        case 7: {
            uint16_t vec_hi = (cpu->active_interrupt == PAT_NMI) ? 0xFFFB : 0xFFFF;
            cpu->addr |= bus_read(bus, vec_hi) << 8;
            cpu->pc = cpu->addr;
            cpu->active_interrupt = 0;
            cpu->cycle = 0;
            break;
        }
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
}
