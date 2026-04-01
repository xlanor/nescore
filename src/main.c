#include "cpu/cpu.h"
#include "bus/bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.nes>\n", argv[0]);
        return 1;
    }
    Bus bus;
    bus_init(&bus);

    if (load_rom(&bus, argv[1]) != 0) {
        return 1;
    }

    int nestest = 0;
    if (argc >= 3 && strcmp(argv[2], "--nestest") == 0) {
        nestest = 1;
    }

    CPU cpu;
    cpu_init(&cpu);
    cpu.bus = &bus;
    cpu_reset(&cpu);

    if (nestest) {
        cpu.pc = 0xC000;
        for (int i = 0; i < 8991; i++) {
            cpu_trace(&cpu);
            cpu_step(&cpu);
        }
    } else {
        printf("PC starts at: %04X\n", cpu.pc);
        for (int i = 0; i < 100; i++) {
            cpu_step(&cpu);
        }
    }

    return 0;
}
