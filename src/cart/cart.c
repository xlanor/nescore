#ifndef CART_C
#define CART_C

#include "cart.h"
#include <stdio.h>
#include <stdlib.h>

#define PRG_BANK_SIZE  16384
#define CHR_BANK_SIZE  8192
// https://www.emulationonline.com/systems/nes/ines-loading/
// https://www.reddit.com/r/EmuDev/comments/1e5qekn/building_a_nes_emulator_ines_cartridge_loading/
// https://forums.nesdev.org/viewtopic.php?t=10243
// https://www.nesdev.org/wiki/INES

int cart_load(Cart *cart, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return -1;
    }
    // Bytes 0-3: Constant $4E $45 $53 $1A ("NES" + MS-DOS EOF marker)
    uint8_t header[16];
    if (fread(header, 1, 16, f) != 16) {
        fprintf(stderr, "Failed to read iNES header\n");
        fclose(f);
        return -1;
    }
    if (header[0] != 'N' || header[1] != 'E' ||
        header[2] != 'S' || header[3] != 0x1A) {
        fprintf(stderr, "Not a valid iNES file\n");
        fclose(f);
        return -1;
    }
    // Byte 4: PRG ROM size in 16 KB units
    cart->prg_banks = header[4];
    cart->prg_size = cart->prg_banks * PRG_BANK_SIZE;
    // Byte 5: CHR ROM size in 8 KB units (0 = CHR RAM, no ROM)
    cart->chr_banks = header[5];
    cart->chr_size = cart->chr_banks * CHR_BANK_SIZE;

    // Byte 6: Flags 6
    // 76543210
    // ||||||||
    // |||||||+- Nametable arrangement: 0: vertical arrangement ("horizontal mirrored") (CIRAM A10 = PPU A11)
    // |||||||                          1: horizontal arrangement ("vertically mirrored") (CIRAM A10 = PPU A10)
    // ||||||+-- 1: Cartridge contains battery-backed PRG RAM ($6000-7FFF) or other persistent memory
    // |||||+--- 1: 512-byte trainer at $7000-$71FF (stored before PRG data)
    // ||||+---- 1: Alternative nametable layout
    // ++++----- Lower nybble of mapper number

    uint8_t flags6 = header[6];
    cart -> battery = (flags6 & 0x02) != 0;
    cart -> four_screen = (flags6 & 0x08) != 0;
    if(cart->four_screen) {
        cart->mirroring = 0;
    }else {
        cart->mirroring = flags6 & 0x01;
    }
    if (flags6 & 0x04) {
        fseek(f, 512, SEEK_CUR);
    }
    
    // Byte 7: Flags 7
    // 76543210
    // ||||||||
    // |||||||+- 1: VS Unisystem
    // ||||||+-- 1: PlayChoice-10
    // ||||++--- if == 2 (0b10): NES 2.0 format
    // ++++----- Upper nybble of mapper number
    //           mapper = (byte7 & 0xF0) | (byte6 >> 4)
    // Dont think we need 0x02 and 0x04 here. seems to be arcade hardware?
    uint8_t flags7 = header[7];
    // TODO this, no clue what the fuck this is for yet
    cart->nes_two = (flags7 & 0x0C) == 0x08;
    cart->mapper = (flags7 & 0xF0) | (flags6 >> 4);

    // Byte 8: PRG RAM size in 8 KB units (0 = infer 8 KB for compatibility)
    cart->prg_ram_banks = header[8];

    // Byte 9: Flags 9
    // 76543210
    // ||||||||
    // |||||||+- TV system (0 = NTSC, 1 = PAL)
    // +++++++-- reserved, should be zero
    // Though in the official specification, very few emulators honor this bit as virtually no ROM images in circulation make use of it. 

    // Byte 10: Flags 10 (unofficial)
    // 76543210
    // ||||||||
    // ||||||++- TV system (0=NTSC, 2=PAL, 1/3=dual compatible)
    // |||++---- reserved
    // ||+------ PRG RAM absence (0 = present, 1 = absent)
    // |+------- bus conflicts (0 = none, 1 = board has bus conflicts)
    // +-------- reserved
    // This byte is not part of the official specification, and relatively few emulators honor it. 


    // Bytes 11-15: padding, should be zero


    cart->prg_rom = malloc(cart->prg_size);
    if (fread(cart->prg_rom, 1, cart->prg_size, f) != cart->prg_size) {
        fprintf(stderr, "Failed to read PRG-ROM\n");
        free(cart->prg_rom);
        fclose(f);
        return -1;
    }

    if (cart->chr_banks > 0) {
        cart->chr_rom = malloc(cart->chr_size);
        if (fread(cart->chr_rom, 1, cart->chr_size, f) != cart->chr_size) {
            fprintf(stderr, "Failed to read CHR-ROM\n");
            free(cart->prg_rom);
            free(cart->chr_rom);
            fclose(f);
            return -1;
        }
    } else {
        cart->chr_rom = NULL;
    }

    fclose(f);
    return 0;
}

void cart_free(Cart *cart) {
    free(cart->prg_rom);
    free(cart->chr_rom);
}

#endif
