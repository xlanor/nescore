#include "ppu.h"
#include "cart/cart.h"
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

// this is fucking complicated, not sure if I am right here,
// relying on this:
// PPU internal memory map ($0000–$3FFF)
// https://www.nesdev.org/wiki/PPU_memory_map
//
// Address       Size   Description              Mapped by
// $0000-$0FFF   $1000  Pattern table 0          Cartridge
// $1000-$1FFF   $1000  Pattern table 1          Cartridge
// $2000-$23BF   $03C0  Nametable 0              Cartridge
// $23C0-$23FF   $0040  Attribute table 0        Cartridge
// $2400-$27BF   $03C0  Nametable 1              Cartridge
// $27C0-$27FF   $0040  Attribute table 1        Cartridge
// $2800-$2BBF   $03C0  Nametable 2              Cartridge
// $2BC0-$2BFF   $0040  Attribute table 2        Cartridge
// $2C00-$2FBF   $03C0  Nametable 3              Cartridge
// $2FC0-$2FFF   $0040  Attribute table 3        Cartridge
// $3000-$3EFF   $0F00  Mirror of $2000-$2EFF    Cartridge
// $3F00-$3F1F   $0020  Palette RAM              Internal to PPU
// $3F20-$3FFF   $00E0  Mirrors of $3F00-$3F1F   Internal to PPU
//
// NES has 2KB of nametable RAM but address space fits 4 × 1KB slots.
// Two slots are always mirrors of the other two — wired by the cartridge.
//
// Slot  Address  Horizontal  Vertical
//   0   $2000    bank 0      bank 0
//   1   $2400    bank 0      bank 1
//   2   $2800    bank 1      bank 0
//   3   $2C00    bank 1      bank 1
//
// Horizontal: $2000=$2400, $2800=$2C00 (stacked vertically)
// Vertical:   $2000=$2800, $2400=$2C00 (stacked horizontally)
static uint16_t nametable_mirror(PPU *ppu, uint16_t addr) {
    // mask off top 4 bits.
    // keep the last 12 bits.
    // nametable region starts at PPU_NAMETABLE_START all the way to 0x3EFF
    addr &= 0x0FFF;
    // gap between each slot is 2^10
    // to check which slot, divide by 1024 basically
    // and then we should get the slot number correctly
    // between 0 and 3
    // ie: 
    // 0000 1100 0000 0000 >> 10 = 11 (binary) ~= 3 (dec)
    int slot   = addr >> 10;
    // look at nametable 1 for example
    // total size = 0x03C0+ 0x0040 = 0x0400
    // 0x0400 -1 = 0x03FF (valid = 0x0000 - 0x03FF)
    int offset = addr & 0x03FF;

    int bank;
    if (ppu->mirroring == MIRROR_HORIZONTAL)
        bank = slot >> 1;
    else
        bank = slot & 1;

    // ppu vram is basically vram[2048 bytes]
    // so vram[0]->vram[0x3FF] = bank 0
    // vram[0x400]->vram[0x7FF] = bank 1
    // check which bank to start at,
    int bank_index = bank * 0x400;
    // then add offset
    int final_index = bank_index + offset;
    return (uint16_t)final_index;
}

uint8_t ppu_bus_read(PPU *ppu, uint16_t addr) {
    // just ensure that we are always
    // within 14 bits.
    addr &= 0x3FFF;
    // 0x0000 - 0x1FFF, pattern tables 
    if (addr < PPU_NAMETABLE_START) {
        // read chr rom at that address
        return ppu->chr_rom[addr % ppu->chr_size];
    // PPU_NAMETABLE_START-0x3EFF
    } else if (addr < PPU_PALETTE_START) {
        return ppu->vram[nametable_mirror(ppu, addr)];
    } else {
        // Palette RAM (32 bytes, mirrored every 0x20)
        // 0x3F10/0x3F14/0x3F18/0x3F1C are mirrors of PPU_PALETTE_START/0x3F04/0x3F08/0x3F0C
        // 0x13 - check bit 4, 1, 0, check if 4th bit is set, 1, 0 clear
        // aka: 0001 0000 should be the end result
        // if so, map to backdrop
        //   0001 0100   (0x14)
        //  & 1110 1111   (~0x10)
        //  = 0000 0100   (0x04)
        if ((addr & 0x13) == 0x10) addr &= ~0x10;
        // pallete is 32 in size
        return ppu->palette[addr & 0x1F];
    }
}

void ppu_bus_write(PPU *ppu, uint16_t addr, uint8_t val) {
    addr &= 0x3FFF;

    if (addr < PPU_NAMETABLE_START) {
        // CHR-ROM — on mapper 0 this is read-only, ignore
        // TODO: handle mappers with CHR-RAM
    } else if (addr < PPU_PALETTE_START) {
        ppu->vram[nametable_mirror(ppu, addr)] = val;

    } else {
        if ((addr & 0x13) == 0x10) addr &= ~0x10;
        ppu->palette[addr & 0x1F] = val;
    }
}

// https://www.nesdev.org/wiki/PPU_registers
uint8_t ppu_read_register(PPU *ppu, uint8_t reg) {
    switch (reg) {
        case PPU_REG_STATUS: {
            // 7  bit  0
            // ---- ----
            // VSOx xxxx
            // |||| ||||
            // |||+-++++- Open bus (unused)
            // ||+------- Sprite overflow flag
            // |+-------- Sprite 0 hit flag
            // +--------- Vblank flag
            uint8_t ret = ppu->status;
            ppu->status &= ~PPUSTATUS_VBLANK;
            ppu->w = false;
            return ret;
        }

        case PPU_REG_OAMDATA: {
            // 7  bit  0
            // ---- ----
            // DDDD DDDD
            // ++++-++++- OAM data
            return ppu->oam[ppu->oam_addr];
        }

        case PPU_REG_DATA: {
            // 7  bit  0
            // ---- ----
            // DDDD DDDD
            // ++++-++++- VRAM data (buffered)
            uint8_t ret = ppu->data_buf;
            ppu->data_buf = ppu_bus_read(ppu, ppu->v);
            if (ppu->v >= PPU_PALETTE_START) {
                ret = ppu->data_buf;
            }
            ppu->v += (ppu->ctrl & PPUCTRL_VRAM_INC32) ? 32 : 1;
            return ret;
        }

    }

    return 0;
}

void ppu_write_register(PPU *ppu, uint8_t reg, uint8_t val) {
    switch (reg) {

        case PPU_REG_CTRL: {
            // R-M-W
            // clear old nametable bits in t, write new nametable bits from val.
            // 7  bit  0
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
            ppu->ctrl = val;
            ppu->t = ppu->t & 0xF3FF;
            uint16_t new_nametables = (uint16_t)(val & 0x03) << 10;
            ppu->t |= new_nametables;
            break;
        }

        case PPU_REG_MASK: {
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
            ppu->mask = val;
            break;
        }

        case PPU_REG_OAMADDR: {
            // 7  bit  0
            // ---- ----
            // AAAA AAAA
            // ++++-++++- OAM address
            ppu->oam_addr = val;
            break;
        }

        case PPU_REG_OAMDATA: {
            // 7  bit  0
            // ---- ----
            // DDDD DDDD
            // ++++-++++- OAM data (write increments oam_addr)
            ppu->oam[ppu->oam_addr] = val;
            ppu->oam_addr++;
            break;
        }

        case PPU_REG_SCROLL: {
            // Two writes, toggled by w latch
            // 1st write: XXXX XXXX - X scroll
            // 2nd write: YYYY YYYY - Y scroll
            if (!ppu->w) {
                uint8_t fine_x   = val & 0x07;
                uint8_t coarse_x = val >> 3;
                ppu->fine_x = fine_x;
                ppu->t = (ppu->t & 0xFFE0) | coarse_x;
            } else {
                uint16_t coarse_y = (uint16_t)(val >> 3) << 5;
                uint16_t fine_y   = (uint16_t)(val & 0x07) << 12;
                ppu->t = (ppu->t & 0x8C1F) | coarse_y | fine_y;
            }
            ppu->w = !ppu->w;
            break;
        }

        case PPU_REG_ADDR: {
            // Two writes, toggled by w latch
            // 1st write: ..AA AAAA - high byte of VRAM address
            // 2nd write: AAAA AAAA - low byte, then copy t to v
            if (!ppu->w) {
                uint16_t addr_high = (uint16_t)(val & 0x3F) << 8;
                ppu->t = (ppu->t & 0x00FF) | addr_high;
            } else {
                uint16_t addr_low = (uint16_t)(val & 0xFF);
                ppu->t = (ppu->t & 0xFF00) | addr_low;
                ppu->v = ppu->t;
            }
            ppu->w = !ppu->w;
            break;
        }

        case PPU_REG_DATA: {
            // 7  bit  0
            // ---- ----
            // DDDD DDDD
            // ++++-++++- VRAM data, auto-increment v after write
            ppu_bus_write(ppu, ppu->v, val);
            if (ppu->ctrl & PPUCTRL_VRAM_INC32) {
                ppu->v +=32;
            }else{
                ppu->v ++;
            }

            break;
        }
       
    }
}
