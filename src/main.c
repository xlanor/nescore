#include "cpu/cpu.h"
#include "bus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_rom(Bus *bus, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return -1;
    }

    // Read iNES header (16 bytes)
    uint8_t header[16];
    if (fread(header, 1, 16, f) != 16) {
        fprintf(stderr, "Failed to read iNES header\n");
        fclose(f);
        return -1;
    }

    // Verify "NES\x1a" magic
    if (header[0] != 'N' || header[1] != 'E' ||
        header[2] != 'S' || header[3] != 0x1A) {
        fprintf(stderr, "Not a valid iNES file\n");
        fclose(f);
        return -1;
    }

    uint8_t prg_banks = header[4]; // 16KB units
    // uint8_t chr_banks = header[5]; // 8KB units (unused for now)

    // Skip trainer if present
    if (header[6] & 0x04) {
        fseek(f, 512, SEEK_CUR);
    }

    // Load PRG-ROM into 0x8000 (and mirror to 0xC000 if only one bank)
    size_t prg_size = prg_banks * 16384;
    uint8_t *prg = malloc(prg_size);
    if (fread(prg, 1, prg_size, f) != prg_size) {
        fprintf(stderr, "Failed to read PRG-ROM\n");
        free(prg);
        fclose(f);
        return -1;
    }

    for (size_t i = 0; i < prg_size; i++) {
        bus->ram[0x8000 + i] = prg[i];
    }
    if (prg_banks == 1) {
        for (size_t i = 0; i < prg_size; i++) {
            bus->ram[0xC000 + i] = prg[i];
        }
    }

    free(prg);
    fclose(f);
    return 0;
}

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
