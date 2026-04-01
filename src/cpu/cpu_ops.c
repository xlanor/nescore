#include "cpu_ops.h"
#include <stdbool.h>

// Flag helpers

void set_flag(CPU *cpu, uint8_t flag, bool cond) {
    if (cond) {
        cpu->status |= flag;
    } else {
        cpu->status &= ~flag;
    }
}

void set_zn(CPU *cpu, uint8_t val) {
    set_flag(cpu, FLAG_Z, val == 0);
    set_flag(cpu, FLAG_N, val & BIT7);
}

/*
 * LDA - Load Accumulator with Memory
 * M -> A                          N Z C I D V
 *                                 + + - - - -
 */
void op_lda(CPU *cpu) {
    cpu->a = cpu->data;
    set_zn(cpu, cpu->a);
}

void op_ldx(CPU *cpu) {
    cpu->x = cpu->data;
    set_zn(cpu, cpu->x);
}

void op_ldy(CPU *cpu) {
    cpu->y = cpu->data;
    set_zn(cpu, cpu->y);
}

void op_and(CPU *cpu) {
    cpu->a &= cpu->data;
    set_zn(cpu, cpu->a);
}

void op_ora(CPU *cpu) {
    cpu->a |= cpu->data;
    set_zn(cpu, cpu->a);
}

void op_eor(CPU *cpu) {
    cpu->a ^= cpu->data;
    set_zn(cpu, cpu->a);
}

void op_adc(CPU *cpu) {
    uint8_t val = cpu->data;
    uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
    set_flag(cpu, FLAG_C, sum > 0xFF);
    set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
    cpu->a = sum & 0xFF;
    set_zn(cpu, cpu->a);
}

void op_sbc(CPU *cpu) {
    uint8_t val = ~cpu->data;
    uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
    set_flag(cpu, FLAG_C, sum > 0xFF);
    set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
    cpu->a = sum & 0xFF;
    set_zn(cpu, cpu->a);
}

void op_cmp(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->a >= cpu->data);
    set_zn(cpu, (cpu->a - cpu->data) & 0xFF);
}

void op_cpx(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->x >= cpu->data);
    set_zn(cpu, (cpu->x - cpu->data) & 0xFF);
}

void op_cpy(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->y >= cpu->data);
    set_zn(cpu, (cpu->y - cpu->data) & 0xFF);
}

void op_bit(CPU *cpu) {
    uint8_t val = cpu->data;
    set_flag(cpu, FLAG_N, val & FLAG_N);
    set_flag(cpu, FLAG_V, val & FLAG_V);
    set_flag(cpu, FLAG_Z, (cpu->a & val) == 0);
}

void op_lax(CPU *cpu) {
    cpu->a = cpu->data;
    cpu->x = cpu->data;
    set_zn(cpu, cpu->a);
}

// Store ops

void op_sta(CPU *cpu) {
    cpu->data = cpu->a;
}

void op_stx(CPU *cpu) {
    cpu->data = cpu->x;
}

void op_sty(CPU *cpu) {
    cpu->data = cpu->y;
}

void op_sax(CPU *cpu) {
    cpu->data = cpu->a & cpu->x;
}

// RMW ops

void op_inc(CPU *cpu) {
    cpu->data++;
    set_zn(cpu, cpu->data);
}

void op_dec(CPU *cpu) {
    cpu->data--;
    set_zn(cpu, cpu->data);
}

void op_asl(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->data & BIT7);
    cpu->data <<= 1;
    set_zn(cpu, cpu->data);
}

void op_lsr(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->data & BIT0);
    cpu->data >>= 1;
    set_zn(cpu, cpu->data);
}

void op_rol(CPU *cpu) {
    bool old_carry = cpu->status & FLAG_C;
    set_flag(cpu, FLAG_C, cpu->data & BIT7);
    cpu->data = (cpu->data << 1) | old_carry;
    set_zn(cpu, cpu->data);
}

void op_ror(CPU *cpu) {
    bool old_carry = cpu->status & FLAG_C;
    set_flag(cpu, FLAG_C, cpu->data & BIT0);
    cpu->data = (cpu->data >> 1) | (old_carry ? BIT7 : 0);
    set_zn(cpu, cpu->data);
}

// Unofficial RMW

void op_slo(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->data & BIT7);
    cpu->data <<= 1;
    cpu->a |= cpu->data;
    set_zn(cpu, cpu->a);
}

void op_rla(CPU *cpu) {
    bool old_carry = cpu->status & FLAG_C;
    set_flag(cpu, FLAG_C, cpu->data & BIT7);
    cpu->data = (cpu->data << 1) | old_carry;
    cpu->a &= cpu->data;
    set_zn(cpu, cpu->a);
}

void op_sre(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->data & BIT0);
    cpu->data >>= 1;
    cpu->a ^= cpu->data;
    set_zn(cpu, cpu->a);
}

void op_rra(CPU *cpu) {
    // ROR the memory value
    bool old_carry = cpu->status & FLAG_C;
    set_flag(cpu, FLAG_C, cpu->data & BIT0);
    cpu->data = (cpu->data >> 1) | (old_carry ? BIT7 : 0);
    // then ADC with the result
    uint8_t val = cpu->data;
    uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
    set_flag(cpu, FLAG_C, sum > 0xFF);
    set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
    cpu->a = sum & 0xFF;
    set_zn(cpu, cpu->a);
}

void op_dcp(CPU *cpu) {
    cpu->data--;
    set_flag(cpu, FLAG_C, cpu->a >= cpu->data);
    set_zn(cpu, (cpu->a - cpu->data) & 0xFF);
}

void op_isc(CPU *cpu) {
    cpu->data++;
    uint8_t val = ~cpu->data;
    uint16_t sum = cpu->a + val + (cpu->status & FLAG_C ? 1 : 0);
    set_flag(cpu, FLAG_C, sum > 0xFF);
    set_flag(cpu, FLAG_V, (~(cpu->a ^ val) & (cpu->a ^ sum)) & BIT7);
    cpu->a = sum & 0xFF;
    set_zn(cpu, cpu->a);
}

// Accumulator shifts

void op_asl_acc(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->a & BIT7);
    cpu->a <<= 1;
    set_zn(cpu, cpu->a);
}

void op_lsr_acc(CPU *cpu) {
    set_flag(cpu, FLAG_C, cpu->a & BIT0);
    cpu->a >>= 1;
    set_zn(cpu, cpu->a);
}

void op_rol_acc(CPU *cpu) {
    bool old_carry = cpu->status & FLAG_C;
    set_flag(cpu, FLAG_C, cpu->a & BIT7);
    cpu->a = (cpu->a << 1) | old_carry;
    set_zn(cpu, cpu->a);
}

void op_ror_acc(CPU *cpu) {
    bool old_carry = cpu->status & FLAG_C;
    set_flag(cpu, FLAG_C, cpu->a & BIT0);
    cpu->a = (cpu->a >> 1) | (old_carry ? BIT7 : 0);
    set_zn(cpu, cpu->a);
}

// Implied operations

void op_tax(CPU *cpu) {
    cpu->x = cpu->a;
    set_zn(cpu, cpu->x);
}

void op_tay(CPU *cpu) {
    cpu->y = cpu->a;
    set_zn(cpu, cpu->y);
}

void op_txa(CPU *cpu) {
    cpu->a = cpu->x;
    set_zn(cpu, cpu->a);
}

void op_tya(CPU *cpu) {
    cpu->a = cpu->y;
    set_zn(cpu, cpu->a);
}

void op_tsx(CPU *cpu) {
    cpu->x = cpu->sp;
    set_zn(cpu, cpu->x);
}

void op_txs(CPU *cpu) {
    cpu->sp = cpu->x;
}

void op_inx(CPU *cpu) {
    cpu->x++;
    set_zn(cpu, cpu->x);
}

void op_iny(CPU *cpu) {
    cpu->y++;
    set_zn(cpu, cpu->y);
}

void op_dex(CPU *cpu) {
    cpu->x--;
    set_zn(cpu, cpu->x);
}

void op_dey(CPU *cpu) {
    cpu->y--;
    set_zn(cpu, cpu->y);
}

// Flags

void op_clc(CPU *cpu) {
    cpu->status &= ~FLAG_C;
}

void op_sec(CPU *cpu) {
    cpu->status |= FLAG_C;
}

void op_cld(CPU *cpu) {
    cpu->status &= ~FLAG_D;
}

void op_sed(CPU *cpu) {
    cpu->status |= FLAG_D;
}

void op_cli(CPU *cpu) {
    cpu->status &= ~FLAG_I;
}

void op_sei(CPU *cpu) {
    cpu->status |= FLAG_I;
}

void op_clv(CPU *cpu) {
    cpu->status &= ~FLAG_V;
}

// NOP

void op_nop(CPU *cpu) {
    (void)cpu;
}

// Branch ops, set cpu->data to 1 if taken, 0 if not

void op_bcc(CPU *cpu) {
    cpu->data = !(cpu->status & FLAG_C);
}

void op_bcs(CPU *cpu) {
    cpu->data = (cpu->status & FLAG_C) ? 1 : 0;
}

void op_beq(CPU *cpu) {
    cpu->data = (cpu->status & FLAG_Z) ? 1 : 0;
}

void op_bne(CPU *cpu) {
    cpu->data = !(cpu->status & FLAG_Z);
}

void op_bmi(CPU *cpu) {
    cpu->data = (cpu->status & FLAG_N) ? 1 : 0;
}

void op_bpl(CPU *cpu) {
    cpu->data = !(cpu->status & FLAG_N);
}

void op_bvs(CPU *cpu) {
    cpu->data = (cpu->status & FLAG_V) ? 1 : 0;
}

void op_bvc(CPU *cpu) {
    cpu->data = !(cpu->status & FLAG_V);
}

// Stack ops

void op_pha(CPU *cpu) {
    cpu->data = cpu->a;
}

void op_php(CPU *cpu) {
    cpu->data = cpu->status | FLAG_B | FLAG_U;
}

void op_pla(CPU *cpu) {
    cpu->a = cpu->data;
    set_zn(cpu, cpu->a);
}

void op_plp(CPU *cpu) {
    cpu->status = (cpu->data & ~FLAG_B) | FLAG_U;
}

// Unofficial ops

void op_anc(CPU *cpu) {
    cpu->a &= cpu->data;
    set_zn(cpu, cpu->a);
    set_flag(cpu, FLAG_C, cpu->a & BIT7);
}

void op_alr(CPU *cpu) {
    cpu->a &= cpu->data;
    set_flag(cpu, FLAG_C, cpu->a & BIT0);
    cpu->a >>= 1;
    set_zn(cpu, cpu->a);
}

void op_arr(CPU *cpu) {
    cpu->a &= cpu->data;
    bool old_carry = cpu->status & FLAG_C;
    cpu->a = (cpu->a >> 1) | (old_carry ? BIT7 : 0);
    set_zn(cpu, cpu->a);
    set_flag(cpu, FLAG_C, cpu->a & 0x40);
    set_flag(cpu, FLAG_V, ((cpu->a >> 6) ^ (cpu->a >> 5)) & 1);
}

void op_axs(CPU *cpu) {
    uint16_t result = (cpu->a & cpu->x) - cpu->data;
    cpu->x = result & 0xFF;
    set_flag(cpu, FLAG_C, result < 0x100);
    set_zn(cpu, cpu->x);
}
