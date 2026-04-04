#include "ppu.h"

// https://www.nesdev.org/wiki/PPU_rendering
// https://www.nesdev.org/wiki/PPU_scrolling

static void inc_hori(PPU *ppu) {
    // Taken from psuedocode here:
    // https://www.nesdev.org/wiki/PPU_scrolling#Coarse_X_increment
    if ((ppu->v & LOOPY_COARSE_X_MASK) == LOOPY_COARSE_X_MASK) {
        ppu->v &= ~LOOPY_COARSE_X_MASK;    // coarse X = 0
        ppu->v ^= LOOPY_NT_HORI;           // switch horizontal nametable
    } else {
        ppu->v += 1;                       // increment coarse X
    }
}

static void inc_vert(PPU *ppu) {
    // Taken from psuedocode here:
    // https://www.nesdev.org/wiki/PPU_scrolling#Y_increment
    if ((ppu->v & LOOPY_FINE_Y_MASK) != LOOPY_FINE_Y_MASK) {
        ppu->v += LOOPY_FINE_Y_INC;                        // increment fine Y
    } else {
        ppu->v &= ~LOOPY_FINE_Y_MASK;                      // fine Y = 0
        uint16_t y = (ppu->v & LOOPY_COARSE_Y_MASK) >> 5; // coarse Y
        if (y == LOOPY_COARSE_Y_MAX_ROW) {
            y = 0;                                         // coarse Y = 0
            ppu->v ^= LOOPY_NT_VERT;                       // switch vertical nametable
        } else if (y == LOOPY_COARSE_Y_OVERFLOW) {
            y = 0;                                         // coarse Y = 0, no nametable switch
        } else {
            y += 1;                                        // increment coarse Y
        }
        ppu->v = (ppu->v & ~LOOPY_COARSE_Y_MASK) | (y << 5); // put coarse Y back into v
    }
}

static void copy_hori(PPU *ppu) {
    // v: ....A.. ...BCDEF <- t: ....A.. ...BCDEF
    uint16_t loopy_mask = LOOPY_COARSE_X_MASK | LOOPY_NT_HORI;
    ppu->v = (ppu->v & ~loopy_mask) | (ppu->t & loopy_mask);
}

static void copy_vert(PPU *ppu) {
    // v: GHIA.BC DEF..... <- t: GHIA.BC DEF.....
    uint16_t loopy_mask = LOOPY_FINE_Y_MASK | LOOPY_NT_VERT | LOOPY_COARSE_Y_MASK;
    ppu->v = (ppu->v & ~loopy_mask) | (ppu->t & loopy_mask);
}

static void fetch_nt(PPU *ppu) {
    uint16_t nt_addr = (ppu->v & 0x0FFF) | PPU_NAMETABLE_START;
    ppu->nt_byte = ppu_bus_read(ppu, nt_addr);
}

static void fetch_at(PPU *ppu) {
    /**
     * The high bits of v are used for fine Y during rendering, 
     * and addressing nametable data only requires 12 bits, with the high 2 CHR address lines fixed to the 0x2000 region. The address to be fetched during rendering can be deduced from v in the following way:
     * tile address      = 0x2000 | (v & 0x0FFF)
     * attribute address = 
     *  0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07)
     *  NN 1111 YYY XXX
     * || |||| ||| +++-- high 3 bits of coarse X (x/4)
     * || |||| +++------ high 3 bits of coarse Y (y/4)
     * || ++++---------- attribute offset (960 bytes)
     * ++--------------- nametable select
     */
    uint16_t at_addr = PPU_AT_BASE
                     | (ppu->v & 0x0C00)  
                     | ((ppu->v >> 4) & 0x38) 
                     | ((ppu->v >> 2) & 0x07);

    uint8_t at_byte = ppu_bus_read(ppu, at_addr);

    uint8_t coarse_x = ppu->v & LOOPY_COARSE_X_MASK;
    uint8_t coarse_y = (ppu->v & LOOPY_COARSE_Y_MASK) >> 5;
    if (coarse_y & 0x02) at_byte >>= 4;
    if (coarse_x & 0x02) at_byte >>= 2;

    ppu->at_byte = at_byte & 0x03;
}

static uint16_t bg_pattern_addr(PPU *ppu) {
    // https://www.nesdev.org/wiki/PPU_rendering#Tile_and_attribute_fetching
    uint16_t pt_base = (ppu->ctrl & PPUCTRL_BG_TABLE) ? PPU_PT_RIGHT : PPU_PT_LEFT;
    uint8_t  fine_y  = (ppu->v >> 12) & 0x07;   // bits 14-12 of v
    return pt_base + (ppu->nt_byte * PPU_PT_TILE_SIZE) + fine_y;
}

static void fetch_bg_lo(PPU *ppu) {
    ppu->bg_lo = ppu_bus_read(ppu, bg_pattern_addr(ppu));
}

static void fetch_bg_hi(PPU *ppu) {
    ppu->bg_hi = ppu_bus_read(ppu, bg_pattern_addr(ppu) + PPU_PT_PLANE_OFFSET);
}

static void load_shift_registers(PPU *ppu) {
    // https://www.nesdev.org/wiki/PPU_rendering#Picture_region
    // on every 8th dot, pattern and attribute data are transferred into shift registers
    // new tile loads into low 8 bits — high 8 bits are currently rendering
    ppu->bg_shift_lo = (ppu->bg_shift_lo & 0xFF00) | ppu->bg_lo;
    ppu->bg_shift_hi = (ppu->bg_shift_hi & 0xFF00) | ppu->bg_hi;

    // attribute is 2 bits per tile — expand to 8 bits (all pixels in a tile share the same palette)
    ppu->at_shift_lo = (ppu->at_shift_lo & 0xFF00) | ((ppu->at_byte & 0x01) ? 0xFF : 0x00);
    ppu->at_shift_hi = (ppu->at_shift_hi & 0xFF00) | ((ppu->at_byte & 0x02) ? 0xFF : 0x00);
}

static void shift_registers(PPU *ppu) {
    // https://www.nesdev.org/wiki/PPU_rendering#Picture_region
    // on every dot, shift registers shift left by 1 — drives pixel pipeline forward one pixel
    ppu->bg_shift_lo <<= 1;
    ppu->bg_shift_hi <<= 1;
    ppu->at_shift_lo <<= 1;
    ppu->at_shift_hi <<= 1;
}

static uint8_t render_pixel(PPU *ppu) {
    // https://www.nesdev.org/wiki/PPU_rendering#Prerender_scanline_(-1,261)
    // https://www.nesdev.org/wiki/PPU_palettes
    // fine_x selects which bit from the shift registers to use (0=leftmost, 7=rightmost)
    uint16_t mux = 0x8000 >> ppu->fine_x;

    // 2-bit pixel value from pattern shift registers (0=transparent, 1-3=color)
    uint8_t p0 = (ppu->bg_shift_lo & mux) ? 1 : 0;
    uint8_t p1 = (ppu->bg_shift_hi & mux) ? 1 : 0;
    uint8_t pixel = (p1 << 1) | p0;

    // 2-bit palette select from attribute shift registers
    uint8_t a0 = (ppu->at_shift_lo & mux) ? 1 : 0;
    uint8_t a1 = (ppu->at_shift_hi & mux) ? 1 : 0;
    uint8_t palette = (a1 << 1) | a0;

    // pixel==0 is transparent/background — always uses universal background color at $3F00
    uint8_t pal_addr = (pixel == 0) ? 0 : (palette << 2) | pixel;

    // mask to 6 bits — NES palette has 64 entries ($00-$3F)
    return ppu_bus_read(ppu, PPU_PALETTE_START + pal_addr) & 0x3F;
}

static void do_fetch_cycle(PPU *ppu) {
    int phase = (ppu->dot - PPU_DOT_VISIBLE_FIRST) % PPU_FETCH_CYCLE;
    switch (phase) {
    case 1: fetch_nt(ppu);          break;
    case 3: fetch_at(ppu);          break;
    case 5: fetch_bg_lo(ppu);       break;
    case 7: fetch_bg_hi(ppu);
            load_shift_registers(ppu);
            inc_hori(ppu);          break;
    }
}

void ppu_render_dot(PPU *ppu) {
    bool rendering = ppu->mask & (PPUMASK_SHOW_BG | PPUMASK_SHOW_SPRITES);

    bool visible_scanline  = ppu->scanline <= PPU_SCANLINE_VISIBLE_LAST;
    bool prerender_scanline = ppu->scanline == PPU_SCANLINE_PRERENDER;

    bool active_dots   = ppu->dot >= PPU_DOT_VISIBLE_FIRST &&
                         ppu->dot <= PPU_DOT_VISIBLE_LAST;
    bool prefetch_dots = ppu->dot >= PPU_DOT_PREFETCH_FIRST &&
                         ppu->dot <= PPU_DOT_PREFETCH_LAST;

    if (rendering) {

        // shift registers advance every dot during active + prefetch, on both
        // visible and pre-render scanlines — this keeps them in sync with fetches
        if ((visible_scanline || prerender_scanline) &&
            (active_dots || prefetch_dots)) {
            shift_registers(ppu);
        }

        if (visible_scanline && active_dots) {
            uint8_t color = render_pixel(ppu);
            int x = ppu->dot - PPU_DOT_VISIBLE_FIRST;
            int y = ppu->scanline;
            ppu->framebuffer[y * NES_WIDTH + x] = color;
        }

        if ((visible_scanline || prerender_scanline) &&
            (active_dots || prefetch_dots)) {
            do_fetch_cycle(ppu);
        }

        if ((visible_scanline || prerender_scanline) &&
            ppu->dot == PPU_DOT_VISIBLE_LAST) {
            inc_vert(ppu);
        }

        if ((visible_scanline || prerender_scanline) &&
            ppu->dot == PPU_DOT_HORI_COPY) {
            copy_hori(ppu);
        }

        if (prerender_scanline &&
            ppu->dot >= PPU_DOT_VERT_COPY_FIRST &&
            ppu->dot <= PPU_DOT_VERT_COPY_LAST) {
            copy_vert(ppu);
        }
    }
}
