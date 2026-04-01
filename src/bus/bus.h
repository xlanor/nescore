#ifndef BUS_H
#define BUS_H

#include <stdint.h>

typedef void (*bus_tick_fn)(void *ctx);
typedef struct {
    // probably need to change this to match actual NES ram later.
    uint8_t ram[0x10000];
    bus_tick_fn tick;
    void *tick_ctx;
} Bus;

void bus_init(Bus* bus);
uint8_t bus_read(Bus *bus, uint16_t addr);
void bus_write(Bus *bus, uint16_t addr, uint8_t val);

#endif
