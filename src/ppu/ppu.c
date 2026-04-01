#include "ppu.h"
#include <string.h>

#define PPU_DOTS_PER_SCANLINE   341
#define PPU_SCANLINES_PER_FRAME 262
#define PPU_SCANLINE_VBLANK     241
#define PPU_SCANLINE_PRERENDER  261


void ppu_init(PPU *ppu) {
    memset(ppu, 0, sizeof(PPU));
}

void ppu_tick(PPU *ppu) {
    ppu->dot += 1;
    if (ppu->dot >= PPU_DOTS_PER_SCANLINE) {
        ppu->dot = 0;
        ppu->scanline++;
    }
    if (ppu->scanline >= PPU_SCANLINES_PER_FRAME) {
        ppu->scanline = 0;
        ppu->frame ++;
        ppu->odd_frame = !ppu->odd_frame;
        ppu->frame_complete = true;
    }
    // https://www.nesdev.org/wiki/PPU_registers
    // 0x80 for bit 7 (Vblank)
    if(ppu->scanline == PPU_SCANLINE_VBLANK && ppu-> dot == 1) {
        ppu->status |= 0x80;
    }
    if(ppu->scanline == PPU_SCANLINE_PRERENDER && ppu-> dot == 1) {
        ppu->status &= ~0x80;
    }
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
