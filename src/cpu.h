#ifndef CEDA_CPU_H
#define CEDA_CPU_H

#include <Z80.h>
#include <stdbool.h>

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

    zuint8 ix;
    zuint8 iy;
} CpuRegs;

void cpu_init(void);

void cpu_run(void);

void cpu_pause(bool enable);

void cpu_reg(CpuRegs *regs);

void cpu_step(void);

#endif // CEDA_CPU_H
