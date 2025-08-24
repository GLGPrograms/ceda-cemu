#include "cpu.h"

#include "3rd/disassembler.h"
#include "bus.h"
#include "int.h"
#include "monitor.h"
#include "time.h"

#include <string.h>

#include "log.h"

#define CPU_CHUNK_CYCLES    4000
#define CPU_FREQ            4000000 // [Hz]
#define CPU_CHUNK_PERIOD    (CPU_CHUNK_CYCLES * 1000L * 1000L / CPU_FREQ) // [us]
#define CPU_PAUSE_PERIOD    20000 // [us] 20 ms => 50 Hz
#define CPU_INSTRUCTION_NOP (0x00)

static Z80 cpu;
static bool pause = true;
static unsigned long int cycles = 0;
static us_time_t last_update = 0;
static us_time_t update_interval = CPU_PAUSE_PERIOD;

static float perf_value = 0;
static const char *perf_unit = "ips";

static uint8_t cpu_hook(void *context, zuint16 address);

static zuint8 cpu_fetch_opcode(void *context, zuint16 address) {
    (void)context;
    LOG_DEBUGB({
        char mnemonic[256];
        uint8_t blob[16];
        bus_mem_readsome(blob, address, 16);
        disassemble(blob, address, mnemonic, 256);
        LOG_DEBUG("%s: [%04x]:\t%s\n", __func__, address, mnemonic);
    });
    zuint8 data = 0x00;
    if (monitor_checkBreakpoint(address)) {
        cpu.hook = cpu_hook;
        data = Z80_HOOK;
    } else {
        data = bus_mem_read(address);
    }
    return data;
}

static void cpu_performance(float *value, const char **unit) {
    *value = perf_value;
    *unit = perf_unit;
}

static void cpu_update_performance(void) {
    static unsigned long int last_cycles = 0;
    static us_time_t last_time = 0;

    const us_time_t now = time_now_us();

    const us_time_t diff_utime = now - last_time;
    const unsigned long int diff_cycles = cycles - last_cycles;

    perf_value = (float)diff_cycles / ((float)diff_utime / 1000.0F / 1000.0F);

    last_time = now;
    last_cycles = cycles;
}

static void cpu_poll(void) {
    last_update = time_now_us();

    if (pause)
        return;

    cycles += z80_run(&cpu, CPU_CHUNK_CYCLES);
    cpu_update_performance();
}

static long cpu_remaining(void) {
    const us_time_t now = time_now_us();
    const us_time_t next_update = last_update + update_interval;
    const us_time_t diff = next_update - now;
    return diff;
}

void cpu_pause(bool enable) {
    pause = enable;

    if (!pause)
        cpu.hook = NULL;

    if (pause)
        update_interval = CPU_PAUSE_PERIOD;
    else
        update_interval = CPU_CHUNK_PERIOD;
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
    monitor_pass();
    cpu_pause(true);
    z80_run(&cpu, 1);
}

void cpu_goto(zuint16 address) {
    cpu.pc.uint16_value = address;
}

void cpu_int(bool state) {
    z80_int(&cpu, state);
}

static uint8_t cpu_mem_read(void *context, zuint16 address) {
    (void)context;
    if (monitor_checkReadWatchpoint(address)) {
        z80_break(&cpu);
        cpu_pause(true);
    }
    return bus_mem_read(address);
}

static void cpu_mem_write(void *context, ceda_address_t address,
                          uint8_t value) {
    (void)context;
    if (monitor_checkWriteWatchpoint(address, value)) {
        z80_break(&cpu);
        cpu_pause(true);
    }
    bus_mem_write(address, value);
}

static uint8_t cpu_io_in(void *context, zuint16 address) {
    (void)context;
    if (monitor_checkInWatchpoint(address)) {
        z80_break(&cpu);
        cpu_pause(true);
    }
    return bus_io_in((ceda_ioaddr_t)address);
}

static void cpu_io_out(void *context, zuint16 address, zuint8 value) {
    (void)context;
    if (monitor_checkOutWatchpoint(address, value)) {
        z80_break(&cpu);
        cpu_pause(true);
    }
    return bus_io_out((ceda_ioaddr_t)address, value);
}

static uint8_t cpu_int_read(void *context, zuint16 address) {
    (void)context;
    (void)address;
    return int_pop();
}

static uint8_t cpu_hook(void *context, zuint16 address) {
    (void)context;
    (void)address;

    z80_break(&cpu);
    cpu_pause(true);
    return Z80_HOOK;
}

void cpu_init(CEDAModule *mod) {
    // init mod struct
    memset(mod, 0, sizeof(*mod));
    mod->init = cpu_init;
    mod->start = NULL;
    mod->poll = cpu_poll;
    mod->remaining = cpu_remaining;
    mod->cleanup = NULL;
    mod->performance = cpu_performance;

    // init cpu
    memset(&cpu, 0, sizeof(cpu));
    cpu.fetch_opcode = cpu_fetch_opcode;
    cpu.fetch = cpu_mem_read;
    cpu.read = cpu_mem_read;
    cpu.write = cpu_mem_write;
    cpu.inta = cpu_int_read;
    cpu.in = cpu_io_in;
    cpu.out = cpu_io_out;

    z80_power(&cpu, true);
}
