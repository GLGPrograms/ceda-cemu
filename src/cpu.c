#include "cpu.h"

#include "bus.h"
#include "dis/disassembler.h"

#include <string.h>

#include "log.h"

#define CPU_CHUNK_CYCLES 100

static Z80 cpu;
static bool pause = true;

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

    z80_run(&cpu, CPU_CHUNK_CYCLES);
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

    regs->ix = cpu.ix_iy->uint8_values.at_0;
    regs->iy = cpu.ix_iy->uint8_values.at_1;

    regs->sp = cpu.sp.uint16_value;
    regs->pc = cpu.pc.uint16_value;
}

void cpu_step(void) {
    cpu_pause(true);
    z80_run(&cpu, 1);
}