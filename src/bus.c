#include "bus.h"
#include <string.h>

static uint8_t ram[0x10000]; // Flat 64KB for now

void bus_init(void) {
    memset(ram, 0, sizeof(ram));
}

uint8_t bus_read(uint16_t addr) {
    return ram[addr];
}

void bus_write(uint16_t addr, uint8_t val) {
    ram[addr] = val;
}
