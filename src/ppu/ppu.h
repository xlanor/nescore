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
    // Timing
    int      scanline;   // 0-261 (0-239 visible, 240 post, 241-260 vblank, 261 pre-render)
    int      dot;        // 0-340 dots per scanline
    uint64_t frame;
    bool     odd_frame;

    // CPU-facing registers ($2000-$2007)
    // https://www.nesdev.org/wiki/PPU_registers
    // 7  bit  0
    // VPHB SINN - ctrl
    // BGRs bMmG - mask
    // VSOx xxxx - status
    // ---- ----
    // VPHB SINN
    // |||| ||||
    // |||| ||++- Base nametable address
    // |||| |+--- VRAM address increment (0: +1 across, 1: +32 down)
    // |||| +---- Sprite pattern table address (0: $0000, 1: $1000)
    // |||+------ Background pattern table address (0: $0000, 1: $1000)
    // ||+------- Sprite size (0: 8x8, 1: 8x16)
    // |+-------- PPU master/slave select
    // +--------- Vblank NMI enable
    uint8_t ctrl;
    // 7  bit  0
    // ---- ----
    // BGRs bMmG
    // |||| ||||
    // |||| |||+- Greyscale (0: normal color, 1: greyscale)
    // |||| ||+-- 1: Show background in leftmost 8 pixels, 0: Hide
    // |||| |+--- 1: Show sprites in leftmost 8 pixels, 0: Hide
    // |||| +---- 1: Enable background rendering
    // |||+------ 1: Enable sprite rendering
    // ||+------- Emphasize red (green on PAL/Dendy)
    // |+-------- Emphasize green (red on PAL/Dendy)
    // +--------- Emphasize blue
    uint8_t mask;
    uint8_t status;
    // 7  bit  0
    // ---- ----
    // AAAA AAAA
    // ++++-++++- OAM address
    uint8_t oam_addr;

    // Loopy scroll registers
    // https://www.nesdev.org/wiki/PPU_scrolling#PPU_internal_registers
    // yyy NN YYYYY XXXXX
    // ||| || ||||| +++++- coarse X (bits 0-4)
    // ||| || +++++------- coarse Y (bits 5-9)
    // ||| ++------------- nametable select (bits 10-11)
    // +++---------------- fine Y (bits 12-14)
    uint16_t v;       // current VRAM address (15 bits)
    uint16_t t;       // temporary VRAM address / scroll latch (15 bits)
    uint8_t  fine_x;  // fine X scroll (3 bits)
    bool     w;       // write latch: 0=first write, 1=second write

    uint8_t data_buf; // PPUDATA read buffer (reads are delayed by one)

    // Object Attribute Memory (sprites)
    uint8_t oam[256];  // primary OAM: 64 sprites × 4 bytes
    uint8_t oam2[32];  // secondary OAM: 8 sprites for current scanline

    // Internal RAM
    uint8_t vram[2048];   // nametable RAM (2KB, two 1KB banks)
    uint8_t palette[32];  // palette RAM (32 bytes)

    // Background shift registers (fed each tile fetch, consumed each dot)
    uint16_t bg_shift_lo;  // pattern table low bits
    uint16_t bg_shift_hi;  // pattern table high bits
    uint16_t at_shift_lo;  // attribute low bits
    uint16_t at_shift_hi;  // attribute high bits

    // Tile fetch latches (filled during fetch, loaded into shift registers)
    uint8_t nt_byte;  // nametable byte
    uint8_t at_byte;  // attribute byte
    uint8_t bg_lo;    // pattern table low byte
    uint8_t bg_hi;    // pattern table high byte

    // Output
    uint8_t framebuffer[NES_WIDTH * NES_HEIGHT];
    bool    frame_complete;

    // NMI signalling to CPU
    bool nmi_output;   // PPUCTRL bit 7 (NMI enable)
    bool nmi_occurred; // PPUSTATUS bit 7 (VBlank set)
    bool nmi_prev;     // previous NMI state (edge detection)

    // Cartridge-supplied data
    uint8_t *chr_rom;
    uint32_t chr_size;
    uint8_t  mirroring;  // MIRROR_HORIZONTAL or MIRROR_VERTICAL
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
