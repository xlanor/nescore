#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#define NES_WIDTH  256
#define NES_HEIGHT 240

// CPU-facing register indices (offset from $2000)
// https://www.nesdev.org/wiki/PPU_registers
#define PPU_REG_CTRL      0
#define PPU_REG_MASK      1
#define PPU_REG_STATUS    2
#define PPU_REG_OAMADDR   3
#define PPU_REG_OAMDATA   4
#define PPU_REG_SCROLL    5
#define PPU_REG_ADDR      6
#define PPU_REG_DATA      7

// PPUCTRL ($2000) bits
#define PPUCTRL_NMI_ENABLE    0x80  // Generate NMI on VBlank
#define PPUCTRL_MASTER_SLAVE  0x40  // PPU master/slave (unused in NES)
#define PPUCTRL_SPRITE_8X16   0x20  // 0=8x8 sprites, 1=8x16 sprites
#define PPUCTRL_BG_TABLE      0x10  // Background pattern table (0=$0000, 1=$1000)
#define PPUCTRL_SPRITE_TABLE  0x08  // Sprite pattern table (0=$0000, 1=$1000)
#define PPUCTRL_VRAM_INC32    0x04  // VRAM address increment (0=+1 across, 1=+32 down)
#define PPUCTRL_NAMETABLE     0x03  // Base nametable address bits

// PPUMASK ($2001) bits
#define PPUMASK_EMPHASIZE_B   0x80
#define PPUMASK_EMPHASIZE_G   0x40
#define PPUMASK_EMPHASIZE_R   0x20
#define PPUMASK_SHOW_SPRITES  0x10  // Show sprites
#define PPUMASK_SHOW_BG       0x08  // Show background
#define PPUMASK_CLIP_SPRITES  0x04  // Hide sprites in leftmost 8px column
#define PPUMASK_CLIP_BG       0x02  // Hide background in leftmost 8px column
#define PPUMASK_GREYSCALE     0x01

// PPUSTATUS ($2002) bits
#define PPUSTATUS_VBLANK      0x80  // VBlank has started
#define PPUSTATUS_SPRITE0_HIT 0x40  // Sprite 0 hit
#define PPUSTATUS_OVERFLOW    0x20  // Sprite overflow

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

// CPU-facing register access (called by bus for $2000–$2007)
uint8_t ppu_read_register(PPU *ppu, uint8_t reg);
void    ppu_write_register(PPU *ppu, uint8_t reg, uint8_t val);

// PPU's own internal memory map ($0000–$3FFF)
// Used during rendering to fetch tiles, attributes, palette
uint8_t ppu_bus_read(PPU *ppu, uint16_t addr);
void    ppu_bus_write(PPU *ppu, uint16_t addr, uint8_t val);

#endif
