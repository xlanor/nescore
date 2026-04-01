#include "bus.h"
#include <string.h>


void bus_init(Bus * bus) {
    memset(bus->ram, 0, sizeof(bus->ram));
    bus ->tick = NULL;
    bus->tick_ctx = NULL;
}

uint8_t bus_read(Bus *bus, uint16_t addr) {
    if (bus->tick) bus->tick(bus->tick_ctx);
    return bus->ram[addr];
}

void bus_write(Bus *bus, uint16_t addr, uint8_t val) {
    if (bus->tick) bus->tick(bus->tick_ctx);
    bus->ram[addr] = val;
}
