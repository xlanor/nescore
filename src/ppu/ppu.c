#include "ppu.h"
#include <string.h>

void ppu_init(PPU *ppu) {
    memset(ppu, 0, sizeof(PPU));
}

void ppu_tick(PPU *ppu) {
    (void)ppu;
}

uint8_t ppu_read_register(PPU *ppu, uint8_t reg) {
    (void)ppu;
    (void)reg;
    return 0;
}

void ppu_write_register(PPU *ppu, uint8_t reg, uint8_t val) {
    (void)ppu;
    (void)reg;
    (void)val;
}
