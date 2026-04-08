#include "nes.h"
#include <stdio.h>

static void nes_tick(void *ctx) {
    NES *nes = (NES *)ctx;
    nes->cpu.cycles++;
    ppu_tick(&nes->ppu);
    ppu_tick(&nes->ppu);
    ppu_tick(&nes->ppu);
    if (nes->ppu.nmi_occurred) {
        if (nes->ppu.ctrl & PPUCTRL_NMI_ENABLE) {
            nes->cpu.nmi_pending = true;
        }
        nes->ppu.nmi_occurred = false;
    }
}

void nes_init(NES *nes) {
    bus_init(&nes->bus);
    cpu_init(&nes->cpu);
    ppu_init(&nes->ppu);
    nes->cpu.bus = &nes->bus;
    nes->bus.ppu = &nes->ppu;
    nes->bus.tick = nes_tick;
    nes->bus.tick_ctx = nes;
}

int nes_load_rom(NES *nes, const char *path) {
    int result = cart_load(&nes->cart, path);
    if (result == 0) {
        nes->bus.cart     = &nes->cart;
        nes->ppu.chr_rom  = nes->cart.chr_rom;
        nes->ppu.chr_size = nes->cart.chr_size;
        nes->ppu.mirroring = nes->cart.mirroring;
    }
    return result;
}

void nes_reset(NES *nes) {
    cpu_reset(&nes->cpu);
    // runs the 7-cycle reset sequence, ticks PPU 21 times 
    cpu_step(&nes->cpu);
}

void nes_step_frame(NES *nes) {
    nes->ppu.frame_complete = false;
    uint8_t dma_byte  = 0;
    int dma_index = 0;
    while (!nes->ppu.frame_complete) {
        if (!nes->bus.dma_pending){
            cpu_tick(&nes->cpu);
        }
        else {
            // DMA - 513 tick cycles on PPU
            // PPU ticks 3x for every cpu tick, tick as normal.
            ppu_tick(&nes->ppu);
            ppu_tick(&nes->ppu);
            ppu_tick(&nes->ppu);
            nes->cpu.cycles++;
            // 513 is alignment
            if(nes->bus.dma_cycles != 513) {
                if(nes->bus.dma_cycles %2 == 0) {
                    // reads
                    dma_byte = nes->bus.ram[nes->bus.dma_page * 256 + dma_index];
                } else {
                    // writes
                    nes->bus.ppu->oam[dma_index] = dma_byte;
                    dma_index++;
                }
            }
            nes->bus.dma_cycles--;
            if (nes->bus.dma_cycles == 0) {
                nes->bus.dma_pending = false;
            }
        }
      
    }
}

const uint8_t *nes_get_framebuffer(NES *nes) {
    return nes->ppu.framebuffer;
}

void nes_trace(NES *nes) {
    printf("%04X  A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%llu\n",
        nes->cpu.pc,
        nes->cpu.a,
        nes->cpu.x,
        nes->cpu.y,
        nes->cpu.status,
        nes->cpu.sp,
        nes->ppu.scanline,
        nes->ppu.dot,
        (unsigned long long)nes->cpu.cycles);
}
