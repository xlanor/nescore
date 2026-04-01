#include "nes.h"

static void nes_tick(void *ctx) {
    NES *nes = (NES *)ctx;
    nes->cpu.cycles++;
    ppu_tick(&nes->ppu);
    ppu_tick(&nes->ppu);
    ppu_tick(&nes->ppu);
}

void nes_init(NES *nes) {
    bus_init(&nes->bus);
    cpu_init(&nes->cpu);
    ppu_init(&nes->ppu);
    nes->cpu.bus = &nes->bus;
    nes->bus.tick = nes_tick;
    nes->bus.tick_ctx = nes;
}

int nes_load_rom(NES *nes, const char *path) {
    return cart_load(&nes->cart, path);
}

void nes_reset(NES *nes) {
    cpu_reset(&nes->cpu);
}

void nes_step_frame(NES *nes) {
    nes->ppu.frame_complete = false;
    while (!nes->ppu.frame_complete) {
        cpu_tick(&nes->cpu);
    }
}

const uint8_t *nes_get_framebuffer(NES *nes) {
    return nes->ppu.framebuffer;
}
