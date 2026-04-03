#ifndef CART_H
#define CART_H

#include <stdint.h>
#include <stdbool.h>

#define MIRROR_HORIZONTAL 0
#define MIRROR_VERTICAL   1

typedef struct {
    uint8_t  *prg_rom;
    uint32_t  prg_size;
    uint8_t  *chr_rom;
    uint32_t  chr_size;
    uint8_t   prg_banks;
    uint8_t   chr_banks;
    uint8_t   mapper;
    uint8_t   mirroring;
    bool      four_screen;
    bool      battery;
    bool      nes_two;
    uint8_t   prg_ram_banks;
    uint32_t  prg_ram_size;
    uint8_t   timing;
} Cart;

int  cart_load(Cart *cart, const char *path);
void cart_free(Cart *cart);

#endif
