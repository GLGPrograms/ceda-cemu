#include "cpu.h"

#include "3rd/disassembler.h"
#include "bus.h"

#include <string.h>

#include "log.h"

#define CPU_CHUNK_CYCLES 100

static Z80 cpu;
static bool pause = true;

#define CPU_BREAKPOINTS 8
static CpuBreakpoint breakpoints[CPU_BREAKPOINTS] = {0};
static unsigned int valid_breakpoints = 0;

/**
 * @brief Check if a breakpoint has been hit.
 *
 * @return true if a breakpoint has been hit, false otherwise.
 */
static bool cpu_checkBreakpoints(void) {
    for (size_t i = 0; i < CPU_BREAKPOINTS; ++i) {
        if (!breakpoints[i].valid)
            continue;

        if (cpu.pc.uint16_value == breakpoints[i].address)
            return true;
    }

    return false;
}

static zuint8 cpu_fetch_opcode(void *context, zuint16 address) {
    LOG_DEBUGB({
        char mnemonic[256];
        uint8_t blob[16];
        bus_mem_readsome(context, blob, address, 16);
        disassemble(blob, address, mnemonic, 256);
        LOG_DEBUG("%s: [%04x]:\t%s\n", __func__, address, mnemonic);
    });
    return bus_mem_read(context, address);
}

void cpu_init(void) {
    memset(&cpu, 0, sizeof(cpu));
    cpu.fetch_opcode = cpu_fetch_opcode;
    cpu.fetch = bus_mem_read;
    cpu.read = bus_mem_read;
    cpu.write = bus_mem_write;
    cpu.in = bus_io_in;
    cpu.out = bus_io_out;

    z80_power(&cpu, true);
}

void cpu_run(void) {
    if (pause)
        return;

    // check if a breakpoint has been hit
    if (valid_breakpoints != 0) {
        if (cpu_checkBreakpoints()) {
            cpu_pause(true);
            // TODO: signal the user that the breakpoint has been hit
            return;
        }
    }

    // if there are breakpoints, step one instruction at a time
    unsigned int cycles = (valid_breakpoints == 0) ? CPU_CHUNK_CYCLES : 1;

    z80_run(&cpu, cycles);
}

void cpu_pause(bool enable) {
    pause = enable;
}

void cpu_reg(CpuRegs *regs) {
    if (regs == NULL)
        return;

    regs->fg.af = cpu.af.uint16_value;
    regs->fg.bc = cpu.bc.uint16_value;
    regs->fg.de = cpu.de.uint16_value;
    regs->fg.hl = cpu.hl.uint16_value;

    regs->bg.af = cpu.af_.uint16_value;
    regs->bg.bc = cpu.bc_.uint16_value;
    regs->bg.de = cpu.de_.uint16_value;
    regs->bg.hl = cpu.hl_.uint16_value;

    regs->ix = cpu.ix_iy[0].uint16_value;
    regs->iy = cpu.ix_iy[1].uint16_value;

    regs->sp = cpu.sp.uint16_value;
    regs->pc = cpu.pc.uint16_value;
}

void cpu_step(void) {
    cpu_pause(true);
    z80_run(&cpu, 1);
}

void cpu_goto(zuint16 address) {
    cpu.pc.uint16_value = address;
}

bool cpu_addBreakpoint(zuint16 address) {
    // find free breakpoint slot (if any) and add it
    for (size_t i = 0; i < CPU_BREAKPOINTS; ++i) {
        if (!breakpoints[i].valid) {
            breakpoints[i].address = address;
            breakpoints[i].valid = true;
            ++valid_breakpoints;
            return true;
        }
    }

    return false;
}

bool cpu_deleteBreakpoint(unsigned int index) {
    if (index >= CPU_BREAKPOINTS)
        return false;

    breakpoints[index].valid = false;
    return true;
}

size_t cpu_getBreakpoints(CpuBreakpoint *v[]) {
    *v = breakpoints;
    return CPU_BREAKPOINTS;
}
