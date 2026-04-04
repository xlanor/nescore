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
    (void)ppu;
}

static void copy_vert(PPU *ppu) {
    (void)ppu;
}

static void fetch_nt(PPU *ppu) {
    (void)ppu;
}

static void fetch_at(PPU *ppu) {
    (void)ppu;
}

static void fetch_bg_lo(PPU *ppu) {
    (void)ppu;
}

static void fetch_bg_hi(PPU *ppu) {
    (void)ppu;
}

static void load_shift_registers(PPU *ppu) {
    (void)ppu;
}

static void shift_registers(PPU *ppu) {
    (void)ppu;
}

static uint8_t render_pixel(PPU *ppu) {
    (void)ppu;
    return 0;
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

        if (visible_scanline && active_dots) {
            shift_registers(ppu);
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
