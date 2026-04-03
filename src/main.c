#include "nes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.nes> [--nestest]\n", argv[0]);
        return 1;
    }

    NES nes;
    nes_init(&nes);

    if (nes_load_rom(&nes, argv[1]) != 0) {
        return 1;
    }

    int nestest = 0;
    if (argc >= 3 && strcmp(argv[2], "--nestest") == 0) {
        nestest = 1;
    }

    nes_reset(&nes);

    if (nestest) {
        nes.cpu.pc = 0xC000;
        for (int i = 0; i < 8991; i++) {
            nes_trace(&nes);
            cpu_step(&nes.cpu);
        }
    } else {
        printf("PC starts at: %04X\n", nes.cpu.pc);
        for (int i = 0; i < 100; i++) {
            cpu_step(&nes.cpu);
        }
    }

    return 0;
}
