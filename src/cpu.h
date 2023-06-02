#ifndef CEDA_CPU_H
#define CEDA_CPU_H

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

    zuint8 ix;
    zuint8 iy;
} CpuRegs;

typedef struct CpuBreakpoint {
    bool valid;
    zuint16 address;
} CpuBreakpoint;

void cpu_init(void);

void cpu_run(void);

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
 * @brief Add a cpu breakpoint.
 *
 * The breakpoint will pause the cpu when the cpu tries to fetch the instruction
 * located at the given address.
 *
 * There is a finite number of breakpoints which can be set.
 *
 * @param address Address of the instruction which must trigger the breakpoint.
 * @return true if the breakpoint has been set, false otherwise.
 */
bool cpu_addBreakpoint(zuint16 address);

bool cpu_deleteBreakpoint(unsigned int index);

size_t cpu_getBreakpoints(CpuBreakpoint *v[]);

#endif // CEDA_CPU_H
