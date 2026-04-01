#include "bus.h"
#include <string.h>

// CPU memory map (https://www.nesdev.org/wiki/CPU_memory_map)
// $0000-$07FF  $0800  2 KB internal RAM
// $0800-$0FFF  $0800  Mirror of $0000-$07FF
// $1000-$17FF  $0800  Mirror of $0000-$07FF
// $1800-$1FFF  $0800  Mirror of $0000-$07FF
// $2000-$2007  $0008  NES PPU registers
// $2008-$3FFF  $1FF8  Mirrors of $2000-$2007 (repeats every 8 bytes)
// $4000-$4017  $0018  NES APU and I/O registers
// $4018-$401F  $0008  APU and I/O functionality (normally disabled, CPU test mode)
// $4020-$FFFF  $BFE0  Cartridge space
// $6000-$7FFF  $2000  Usually cartridge RAM (PRG-RAM), when present
// $8000-$FFFF  $8000  Usually cartridge ROM (PRG-ROM) and mapper registers

void bus_init(Bus * bus) {
    memset(bus->ram, 0, sizeof(bus->ram));
    bus ->tick = NULL;
    bus->tick_ctx = NULL;
    bus->cart = NULL;
}

uint8_t bus_read(Bus *bus, uint16_t addr) {
    if (bus->tick) bus->tick(bus->tick_ctx);

    if (addr <= 0x1FFF) {
        return bus->ram[addr & 0x07FF];
    } else if (addr <= 0x3FFF) {
        // TODO: PPU registers (addr & 0x2007)
        return 0;
    } else if (addr <= 0x4017) {
        // TODO: APU and I/O registers
        return 0;
    } else if (addr <= 0x401F) {
        // TODO: CPU test mode 
        return 0;
    } else if (addr <= 0x5FFF) {
        // TODO: cartridge space, mapper-specific
        return 0;
    } else if (addr <= 0x7FFF) {
        // TODO: PRG-RAM ($6000-$7FFF)
        return 0;
    } else {
        return bus->cart->prg_rom[(addr - 0x8000) % bus->cart->prg_size];
    }
}

void bus_write(Bus *bus, uint16_t addr, uint8_t val) {
    if (bus->tick) bus->tick(bus->tick_ctx);

    if (addr <= 0x1FFF) {
        bus->ram[addr & 0x07FF] = val;
    } else if (addr <= 0x3FFF) {
        // TODO: PPU registers
    } else if (addr <= 0x4017) {
        // TODO: APU and I/O registers
    } else if (addr <= 0x401F) {
        // TODO: CPU test mode
    } else if (addr <= 0x5FFF) {
        // TODO: cartridge space, mapper-specific
    } else if (addr <= 0x7FFF) {
        // TODO: PRG-RAM
    } else {
        // cart ROM is read-only, writes ignored
    }
}

