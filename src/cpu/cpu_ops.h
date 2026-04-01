#ifndef CPU_OPS_H
#define CPU_OPS_H

#include "cpu.h"
typedef enum {
    PAT_IMPLIED, 
    PAT_IMMEDIATE,
    PAT_ZPG_READ, 
    PAT_ZPG_WRITE, 
    PAT_ZPG_RMW,
    PAT_ZPX_READ, 
    PAT_ZPX_WRITE, 
    PAT_ZPX_RMW,
    PAT_ZPY_READ,
    PAT_ZPY_WRITE,
    PAT_ABS_READ, 
    PAT_ABS_WRITE, 
    PAT_ABS_RMW,
    PAT_ABX_READ, 
    PAT_ABX_WRITE, 
    PAT_ABX_RMW,
    PAT_ABY_READ, 
    PAT_ABY_WRITE, 
    PAT_ABY_RMW,
    PAT_IZX_READ, 
    PAT_IZX_WRITE, 
    PAT_IZX_RMW,
    PAT_IZY_READ, 
    PAT_IZY_WRITE, 
    PAT_IZY_RMW,
    PAT_BRANCH,
    PAT_PUSH, 
    PAT_PULL,
    PAT_JSR, 
    PAT_RTS, 
    PAT_RTI, 
    PAT_BRK,
    PAT_JMP_ABS, 
    PAT_JMP_IND,
} CyclePattern;

typedef void (*op_fn)(CPU *cpu);

typedef struct {
    CyclePattern pattern;
    op_fn execute;
} OpcodeEntry;

extern const OpcodeEntry opcode_table[256];

// Flag helpers
void set_flag(CPU *cpu, uint8_t flag, _Bool cond);
void set_zn(CPU *cpu, uint8_t val);

// Read operations
void op_lda(CPU *cpu);
void op_ldx(CPU *cpu);
void op_ldy(CPU *cpu);
void op_and(CPU *cpu);
void op_ora(CPU *cpu);
void op_eor(CPU *cpu);
void op_adc(CPU *cpu);
void op_sbc(CPU *cpu);
void op_cmp(CPU *cpu);
void op_cpx(CPU *cpu);
void op_cpy(CPU *cpu);
void op_bit(CPU *cpu);
void op_lax(CPU *cpu);

// Store operations
void op_sta(CPU *cpu);
void op_stx(CPU *cpu);
void op_sty(CPU *cpu);
void op_sax(CPU *cpu);

// Read-Modify-Write
void op_inc(CPU *cpu);
void op_dec(CPU *cpu);
void op_asl(CPU *cpu);
void op_lsr(CPU *cpu);
void op_rol(CPU *cpu);
void op_ror(CPU *cpu);
void op_slo(CPU *cpu);
void op_rla(CPU *cpu);
void op_sre(CPU *cpu);
void op_rra(CPU *cpu);
void op_dcp(CPU *cpu);
void op_isc(CPU *cpu);

// Accumulator shifts
void op_asl_acc(CPU *cpu);
void op_lsr_acc(CPU *cpu);
void op_rol_acc(CPU *cpu);
void op_ror_acc(CPU *cpu);

// Implied register operations
void op_tax(CPU *cpu);
void op_tay(CPU *cpu);
void op_txa(CPU *cpu);
void op_tya(CPU *cpu);
void op_tsx(CPU *cpu);
void op_txs(CPU *cpu);
void op_inx(CPU *cpu);
void op_iny(CPU *cpu);
void op_dex(CPU *cpu);
void op_dey(CPU *cpu);

// Flag operations
void op_clc(CPU *cpu);
void op_sec(CPU *cpu);
void op_cld(CPU *cpu);
void op_sed(CPU *cpu);
void op_cli(CPU *cpu);
void op_sei(CPU *cpu);
void op_clv(CPU *cpu);

// NOP
void op_nop(CPU *cpu);

// Branches
void op_bcc(CPU *cpu);
void op_bcs(CPU *cpu);
void op_beq(CPU *cpu);
void op_bne(CPU *cpu);
void op_bmi(CPU *cpu);
void op_bpl(CPU *cpu);
void op_bvs(CPU *cpu);
void op_bvc(CPU *cpu);

// Stack
void op_pha(CPU *cpu);
void op_php(CPU *cpu);
void op_pla(CPU *cpu);
void op_plp(CPU *cpu);

// Immediate-only unofficial
void op_anc(CPU *cpu);
void op_alr(CPU *cpu);
void op_arr(CPU *cpu);
void op_axs(CPU *cpu);

#endif
