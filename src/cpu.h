#ifndef CEDA_CPU_H
#define CEDA_CPU_H

#include "module.h"

#include <Z80.h>
#include <stdbool.h>
#include <stddef.h>

#define CPU_MAX_OPCODE_LEN 6

typedef struct CpuGenRegs {
    zuint16 af;
    zuint16 bc;
    zuint16 de;
    zuint16 hl;
} CpuGenRegs;
typedef struct CpuRegs {
    CpuGenRegs fg;
    CpuGenRegs bg;

    zuint16 sp;
    zuint16 pc;

    zuint16 ix;
    zuint16 iy;
} CpuRegs;

void cpu_init(CEDAModule *mod);

void cpu_pause(bool enable);
void cpu_reg(CpuRegs *regs);
void cpu_step(void);

/**
 * @brief Move the cpu program counter to the given address.
 *
 * @param address Target address.
 */
void cpu_goto(zuint16 address);

/**
 * @brief Set the interrupt line of the Z80 CPU.
 *
 * @param state true to assert IRQ, false to de-assert.
 */
void cpu_int(bool state);

#endif // CEDA_CPU_H
