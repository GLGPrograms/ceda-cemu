#include "cpu.h"

#include "bus.h"

#include <string.h>

#define CPU_CHUNK_CYCLES 100

static Z80 cpu;

void cpu_init(void) {
    memset(&cpu, 0, sizeof(cpu));
    cpu.fetch_opcode = bus_read;
    cpu.fetch = bus_read;
    cpu.read = bus_read;
    cpu.write = bus_write;

    z80_power(&cpu, TRUE);
}

void cpu_run(void) {
    z80_run(&cpu, CPU_CHUNK_CYCLES);
}

