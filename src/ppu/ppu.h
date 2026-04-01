#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#define NES_WIDTH  256
#define NES_HEIGHT 240

typedef struct {
    int scanline;
    int dot;
    uint64_t frame;
    bool odd_frame;

    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    uint8_t oam_addr;

    uint16_t v;
    uint16_t t;
    uint8_t  fine_x;
    bool     w;

    uint8_t data_buf;

    uint8_t oam[256];
    uint8_t oam2[32];

    uint8_t vram[2048];
    uint8_t palette[32];

    uint16_t bg_shift_lo;
    uint16_t bg_shift_hi;
    uint16_t at_shift_lo;
    uint16_t at_shift_hi;

    uint8_t nt_byte;
    uint8_t at_byte;
    uint8_t bg_lo;
    uint8_t bg_hi;

    uint8_t framebuffer[NES_WIDTH * NES_HEIGHT];
    bool frame_complete;

    bool nmi_output;
    bool nmi_occurred;
    bool nmi_prev;

    uint8_t *chr_rom;
    uint32_t chr_size;
    uint8_t mirroring;
} PPU;

void    ppu_init(PPU *ppu);
void    ppu_tick(PPU *ppu);
uint8_t ppu_read_register(PPU *ppu, uint8_t reg);
void    ppu_write_register(PPU *ppu, uint8_t reg, uint8_t val);

#endif
