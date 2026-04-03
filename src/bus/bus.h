#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include "cart/cart.h"

// Forward declaration — avoids circular include with ppu.h
struct PPU;

typedef void (*bus_tick_fn)(void *ctx);
typedef struct {
    uint8_t      ram[2048];
    bus_tick_fn  tick;
    void        *tick_ctx;
    Cart        *cart;
    struct PPU  *ppu;    // routes $2000–$3FFF to PPU registers
} Bus;

void bus_init(Bus* bus);
uint8_t bus_read(Bus *bus, uint16_t addr);
void bus_write(Bus *bus, uint16_t addr, uint8_t val);

#endif
