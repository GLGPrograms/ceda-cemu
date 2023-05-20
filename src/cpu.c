#include "cpu.h"

#include "bus.h"
#include "dis/disassembler.h"

#include <string.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

#define CPU_CHUNK_CYCLES 100

static Z80 cpu;

static zuint8 cpu_fetch_opcode(void *context, zuint16 address) {
    LOG_DEBUGB({
            char mnemonic[256];
            uint8_t blob[16];
            bus_readsome(context, blob, address, 16);
            int r = disassemble2(blob, address, mnemonic, 256);
            LOG_DEBUG("[%04x]:\t%s\n", address, mnemonic);
    });
    return bus_read(context, address);
}

void cpu_init(void) {
    memset(&cpu, 0, sizeof(cpu));
    cpu.fetch_opcode = cpu_fetch_opcode;
    cpu.fetch = bus_read;
    cpu.read = bus_read;
    cpu.write = bus_write;

    z80_power(&cpu, TRUE);
}

void cpu_run(void) {
    z80_run(&cpu, 1);
}

