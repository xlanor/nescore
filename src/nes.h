#ifndef NES_H
#define NES_H

#include "cpu/cpu.h"
#include "ppu/ppu.h"
#include "bus/bus.h"
#include "cart/cart.h"

typedef struct {
    CPU cpu;
    PPU ppu;
    Bus bus;
    Cart cart;
} NES;

void           nes_init(NES *nes);
int            nes_load_rom(NES *nes, const char *path);
void           nes_reset(NES *nes);
void           nes_step_frame(NES *nes);
const uint8_t *nes_get_framebuffer(NES *nes);

#endif
