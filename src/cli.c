#include "cli.h"

#include "3rd/disassembler.h"
#include "3rd/fifo.h"
#include "bus.h"
#include "ceda_string.h"
#include "cpu.h"
#include "macro.h"
#include "time.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define LOG_FORMAT LOG_FMT_VERBOSE
#define LOG_LEVEL  LOG_LVL_DEBUG
#include "log.h"

static const uint16_t CLI_PORT = 0xceda;

#define USER_PROMPT_STR "> "
enum { LINE_BUFFER_SIZE = 128 };   // small line-like stuff
enum { BLOCK_BUFFER_SIZE = 4096 }; // big page-like stuff

#define USER_BAD_ARG_STR       "bad argument\n"
#define USER_NO_SPACE_LEFT_STR "no space left\n"

static bool initialized = false;
static bool quit = false;
static const us_interval_t UPDATE_INTERVAL = 20000; // [us] 20 ms => 50 Hz
static us_time_t last_update = 0;                   // last poll() call

static int sockfd = -1;
static int connfd = -1;

DECLARE_FIFO_TYPE(ceda_string_t *, TxFifo, 8);
static TxFifo tx_fifo;

bool cli_isQuit(void) {
    return quit;
}

/**
 * @brief Send a string to the connected client.
 *
 * This function takes a null-terminated C-string as argument,
 * and copies its content in a internal buffer.
 *
 * @param str pointer to the null-terminated C-string to send
 */
static void cli_send_string(const char *str) {
    ceda_string_t *message = ceda_string_new(0);
    ceda_string_cpy(message, str);
    FIFO_PUSH(&tx_fifo, message);
}

/**
 * @brief Extract the first word from a null-terminated C string.
 *
 * @param word Pointer to destination null-terminated string.
 * @param src Pointer to input string to inspect.
 * @param size Size of destination word buffer.
 *
 * @return const char* Pointer to first char after the word in the input string.
 * NULL if there are no more words.
 */
static const char *cli_next_word(char *word, const char *src, size_t size) {
    bool started = false;

    assert(src);
    if (*src == '\0') {
        return NULL;
    }

    size_t idx = 0; // index of current char under examination
    size_t len = 0; // toekn/word length
    for (; len < size - 1; ++idx) {
        if (src[idx] == ' ') {
            if (started) {
                break;
            }
            continue;
        }
        started = true;
        word[len++] = src[idx];

        if (src[idx] == '\0') {
            break;
        }
    }

    word[len++] = '\0';
    return src + idx;
}

/**
 * @brief Extract an unsigned int expressed in hex format from a C string.
 *
 * @param dst Pointer to unsigned int to write into.
 * @param src Pointer to input string to inspect.

 * @return const char* Pointer to first char after the unsigned int in the input
 * string. NULL if there has been an error during integer parsing.
 */
static const char *cli_next_hex(unsigned int *dst, const char *src) {
    assert(src);

    char *endptr = NULL;
    char word[LINE_BUFFER_SIZE] = {0};
    src = cli_next_word(word, src, LINE_BUFFER_SIZE);
    if (src == NULL) {
        return NULL;
    }
    // TODO(giomba)
    // sooner or later, better if the handle hex8, hex16, hex32 separately
    *dst = (unsigned int)strtol(word, &endptr, 0x10);
    if (errno == EINVAL || errno == ERANGE || endptr == word) {
        return NULL;
    }

    return src;
}

/**
 * @brief Extract an unsigned int express in dec format from a C string.
 *
 * @param dst Pointer to unsigned int to write into.
 * @param src Pointer to input string to inspect.

 * @return const char* Pointer to first char after the unsigned int in the input
 * string. NULL if there has been an error during integer parsing.
 */
static const char *cli_next_int(unsigned int *dst, const char *src) {
    assert(src);
    static const int base = 10;

    char *endptr = NULL;
    char word[LINE_BUFFER_SIZE] = {0};
    src = cli_next_word(word, src, LINE_BUFFER_SIZE);
    if (src == NULL) {
        return NULL;
    }

    *dst = (unsigned int)strtol(word, &endptr, base);
    if (errno == EINVAL || errno == ERANGE || endptr == word) {
        return NULL;
    }

    return src;
}

static ceda_string_t *cli_quit(const char *arg) {
    (void)arg;

    quit = true;

    return NULL;
}

static ceda_string_t *cli_pause(const char *arg) {
    (void)arg;
    cpu_pause(true);
    return NULL;
}

static ceda_string_t *cli_continue(const char *arg) {
    (void)arg;
    cpu_step(); // possibly step past the breakpoint
    cpu_pause(false);
    return NULL;
}

static ceda_string_t *cli_reg(const char *arg) {
    (void)arg;
    CpuRegs regs;
    cpu_reg(&regs);

    // disassemble current pc
    char _dis[LINE_BUFFER_SIZE];
    uint8_t blob[CPU_MAX_OPCODE_LEN];
    bus_mem_readsome(blob, regs.pc, CPU_MAX_OPCODE_LEN);
    disassemble(blob, regs.pc, _dis, LINE_BUFFER_SIZE);
    const char *dis = _dis;
    while (*dis == ' ') {
        ++dis;
    }

    ceda_string_t *msg = ceda_string_new(BLOCK_BUFFER_SIZE);
    /* don't pretend miracles from the formatter and the linter */
    /* clang-format off */
    ceda_string_printf(msg,
        // string format
        " %s\n"
        " PC   SP   AF   BC   DE   HL   AF'  BC'  DE'  HL'  IX   IY\n"
        "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",

        // varargs
        dis,
        regs.pc, regs.sp, regs.fg.af, regs.fg.bc, regs.fg.de, regs.fg.hl,
        regs.bg.af, regs.bg.bc, regs.bg.de, regs.bg.hl, regs.ix, regs.iy);
    /* clang-format on */

    return msg;
}

static ceda_string_t *cli_step(const char *arg) {
    cpu_step();
    return cli_reg(arg);
}

static ceda_string_t *cli_break(const char *arg) {
    char word[LINE_BUFFER_SIZE];

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract address
    unsigned int _address;
    arg = cli_next_hex(&_address, arg);

    // no address => show current breakpoints
    if (arg == NULL) {
        CpuBreakpoint *breakpoints;
        const size_t countof_breakpoints = cpu_getBreakpoints(&breakpoints);
        int count = 0;
        ceda_string_t *msg = ceda_string_new(0);
        for (size_t i = 0; i < countof_breakpoints; ++i) {
            if (!breakpoints[i].valid)
                continue;
            ++count;
            ceda_string_printf(msg, "%lu\t%04x\n", i, breakpoints[i].address);
        }
        if (count == 0) {
            ceda_string_cpy(msg, "no breakpoint set\n");
        }
        return msg;
    }

    if (_address >= 0x10000) {
        ceda_string_t *msg = ceda_string_new(0);
        ceda_string_cpy(msg, USER_BAD_ARG_STR "address must be 16 bit\n");
        return msg;
    }
    const zuint16 address = (zuint16)_address;

    // actually set breakpoint
    bool ret = cpu_addBreakpoint(address);

    if (!ret) {
        ceda_string_t *msg = ceda_string_new(0);
        ceda_string_cpy(msg, USER_NO_SPACE_LEFT_STR);
        return msg;
    }

    return NULL;
}

static ceda_string_t *cli_delete(const char *arg) {
    char word[LINE_BUFFER_SIZE];

    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    char what[LINE_BUFFER_SIZE];
    // extract what to delete (breakpoint, watchpoint, ...)
    arg = cli_next_word(what, arg, LINE_BUFFER_SIZE);

    // missing what
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing delete target\n");
        return msg;
    }

    // extract index
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // missing index
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing index\n");
        return msg;
    }

    // atoi index
    unsigned int index;
    arg = cli_next_int(&index, word);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "bad index format\n");
        return msg;
    }

    // actually delete something
    if (strcmp(what, "breakpoint") == 0) {
        if (!cpu_deleteBreakpoint(index)) {
            ceda_string_cpy(msg, "can't delete breakpoint\n");
            return msg;
        }
    } else if (strcmp(what, "watchpoint") == 0) {
        // TODO(giomba): implement this for watchpoints
    } else {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "unknown delete target\n");
        return msg;
    }

    // all ok
    ceda_string_delete(msg);
    return NULL;
}

static ceda_string_t *cli_read(const char *arg) {
    char word[LINE_BUFFER_SIZE];
    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract address
    unsigned int address;
    arg = cli_next_hex(&address, arg);

    // missing address
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing address\n");
        return msg;
    }
    // address >= 2^16
    if (address >= 0x10000) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "address must be 16 bit\n");
        return msg;
    }

    // read some mem
    const ceda_size_t BLOB_SIZE = 8 * (size_t)16;
    uint8_t blob[BLOB_SIZE];
    bus_mem_readsome(blob, (zuint16)address, BLOB_SIZE);

    // print nice hexdump
    uint8_t ascii[16 + 1] = {0};
    for (unsigned int i = 0; i < BLOB_SIZE; ++i) {
        const uint8_t c = blob[i];

        if (i % 16 == 0) {
            ceda_string_printf(msg, "%04x\t", address + i);
        }

        ceda_string_printf(msg, "%02x ", ((unsigned int)(c)) & 0xff);
        ascii[i % 16] = isprint(c) ? c : '.';

        if (i % 16 == 8 - 1) {
            ceda_string_printf(msg, " ");
        }

        if (i % 16 == 16 - 1) {
            ceda_string_printf(msg, "\t%s\n", ascii);
        }
    }

    return msg;
}

static ceda_string_t *cli_write(const char *arg) {
    char word[LINE_BUFFER_SIZE];
    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract address
    unsigned int _address;
    arg = cli_next_hex(&_address, arg);
    // missing address
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "bad address format\n");
        return msg;
    }
    // address >= 2^16
    if (_address >= 0x10000) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "address must be 16 bit\n");
        return msg;
    }
    const zuint16 address = (zuint16)_address;

    // read values, and put them in memory
    for (zuint16 i = 0;; ++i) {
        // extract value
        unsigned int _value;
        arg = cli_next_hex(&_value, arg);
        if (arg == NULL) {
            // first value cannot be missing
            if (i == 0) {
                ceda_string_cpy(msg, USER_BAD_ARG_STR "missing value\n");
                return msg;
            }

            // nothing more to write
            break;
        }

        // value >= 2^8
        if (_value >= 0x100) {
            ceda_string_cpy(msg, USER_BAD_ARG_STR "value must be 8 bit\n");
            return msg;
        }
        const zuint8 value = (zuint8)_value;

        bus_mem_write(address + i, value);
    }

    ceda_string_delete(msg);
    return NULL;
}

static ceda_string_t *cli_dis(const char *arg) {
    char word[LINE_BUFFER_SIZE];
    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    unsigned int address;
    arg = cli_next_hex(&address, arg);
    // if no address specified, use current pc
    if (arg == NULL) {
        CpuRegs regs;
        cpu_reg(&regs);
        address = regs.pc;
    }

    int disb = 0; // disassembled bytes
    char line[LINE_BUFFER_SIZE];
    uint8_t blob[CPU_MAX_OPCODE_LEN];
    for (int i = 0; i < 16; ++i) {
        bus_mem_readsome(blob, (zuint16)(address + (unsigned int)disb),
                         CPU_MAX_OPCODE_LEN);
        disb += disassemble(blob, (int)address + disb, line, BLOCK_BUFFER_SIZE);
        ceda_string_printf(msg, "%s\n", line);
    }

    return msg;
}

/**
 * @brief Save a chunk of memory to disk.
 *
 * Expected command line syntax:
 *  save <filename> <start> <end>
 * where
 *  filename: name of the file where to save the dump (no spaces allowed)
 *  start: starting memory address, in hex
 *  end: ending memory address, in hex
 *
 * Data is saved [start;end)
 *
 * Example: dump video memory
 *  save video.crt d000 e000
 *
 * File format: .prg
 * First two octets represent the starting address in little endian,
 * then actual data follows.
 *
 * @param arg Pointer to the command line string.
 *
 * @return char* NULL in case of success, pointer to error message otherwise.
 */
static ceda_string_t *cli_save(const char *arg) {
    char word[LINE_BUFFER_SIZE];
    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract file name
    char filename[LINE_BUFFER_SIZE];
    arg = cli_next_word(filename, arg, LINE_BUFFER_SIZE);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing file name\n");
        return msg;
    }

    unsigned int start_address;
    arg = cli_next_hex(&start_address, arg);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "bad start address\n");
        return msg;
    }

    unsigned int end_address;
    arg = cli_next_hex(&end_address, arg);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "bad end address\n");
        return msg;
    }

    if (start_address >= 0x10000 || end_address >= 0x10000) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "address must be 16 bit\n");
        return msg;
    }
    if (end_address < start_address) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR
                        "end address must be greater than start address\n");
        return msg;
    }

    const ceda_size_t data_size = (ceda_size_t)(end_address - start_address);
    const ceda_size_t alloc_size = data_size + 2;

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        ceda_string_printf(msg, "unable to open file: %.64s\n", filename);
        return msg;
    }

    uint8_t *blob = malloc(alloc_size);
    // header: start_address in little endian
    const uint8_t lsb = start_address & 0xff;
    const uint8_t msb = (start_address >> 8) & 0xff;
    blob[0] = lsb;
    blob[1] = msb;
    // payload
    bus_mem_readsome(&blob[2], (zuint16)start_address, data_size);
    // write
    size_t written = fwrite(blob, 1, alloc_size, fp);
    if (written != alloc_size) {
        ceda_string_printf(msg, "fwrite returned: %lu", written);
        return msg;
    }

    int rclose = fclose(fp);
    free(blob);

    if (rclose != 0) {
        ceda_string_printf(msg, "fclose failed.");
        return msg;
    }

    ceda_string_delete(msg);
    return NULL;
}

/**
 * @brief Load a chunk of memory from disk.
 *
 * Expected command line syntax:
 *  load <filename> [start]
 * where
 *  filename: name of the file from which to load the dump (no spaces allowed)
 *  start: starting memory address, in hex
 *
 * When loading, this routine will use the starting address saves inside the
 * file, unless a starting address is explicitly specified on the command line,
 * in which case, the starting address of the file will be overridden.
 *
 * Example: load video memory dump, but one row below
 *  load video.crt d050
 *
 * File format: .prg
 * First two octets represent the starting address in little endian,
 * then actual data follows.
 *
 * @param arg Pointer to the command line string.
 *
 * @return char* NULL in case of success, pointer to error message otherwise.
 */
static ceda_string_t *cli_load(const char *arg) {
    char word[LINE_BUFFER_SIZE];
    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract filename
    char filename[LINE_BUFFER_SIZE];
    arg = cli_next_word(filename, arg, LINE_BUFFER_SIZE);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing filename\n");
        return msg;
    }

    // extract starting address
    // (optional, override what's inside the file)
    unsigned int address;
    arg = cli_next_hex(&address, arg);
    const bool override_address = arg != NULL;
    if (arg != NULL && address >= 0x10000) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "address must be 16 bit\n");
        return msg;
    }

    // extract starting address from file
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        ceda_string_cpy(msg, "unable to open file\n");
        return msg;
    }
    size_t ret = fread(word, 1, 2, fp);
    if (ret != 2) {
        (void)fclose(fp);
        ceda_string_cpy(msg, "unable to read start address from file\n");
        return msg;
    }
    const ceda_address_t file_address =
        (ceda_address_t)((word[0] & 0xff) | ((word[1] & 0xff) << 8));

    // use starting address from file if we are not overriding it
    if (!override_address) {
        address = file_address;
    }

    // read data until the end, and write it in memory
    for (;;) {
        char c;
        ret = fread(&c, 1, 1, fp);
        if (ret == 0)
            break;
        bus_mem_write((zuint16)address++, (zuint8)c);
    }

    (void)fclose(fp);

    ceda_string_delete(msg);
    return NULL;
}

static ceda_string_t *cli_goto(const char *arg) {
    char word[LINE_BUFFER_SIZE];

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract address and perform sanity check
    unsigned int address;
    arg = cli_next_hex(&address, arg);
    if (arg == NULL) {
        ceda_string_t *msg = ceda_string_new(0);
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing address\n");
        return msg;
    }
    if (address >= 0x10000) {
        ceda_string_t *msg = ceda_string_new(0);
        ceda_string_cpy(msg, USER_BAD_ARG_STR "address must be 16 bit\n");
        return msg;
    }

    // inconditional jump
    cpu_goto((zuint16)address);
    return NULL;
}

static ceda_string_t *cli_in(const char *arg) {
    char word[LINE_BUFFER_SIZE];
    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract io address
    unsigned int address;
    arg = cli_next_hex(&address, arg);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing address\n");
        return msg;
    }
    if (address >= 0x100) {
        ceda_string_cpy(msg, "address must be 8 bit\n");
        return msg;
    }

    const zuint8 value = bus_io_in((ceda_ioaddr_t)address);
    ceda_string_printf(msg, "%02x\n", value);
    return msg;
}

static ceda_string_t *cli_out(const char *arg) {
    char word[LINE_BUFFER_SIZE];
    ceda_string_t *msg = ceda_string_new(0);

    // skip argv[0]
    arg = cli_next_word(word, arg, LINE_BUFFER_SIZE);

    // extract address
    unsigned int address;
    arg = cli_next_hex(&address, arg);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing address\n");
        return msg;
    }
    if (address >= 0x100) {
        ceda_string_cpy(msg, "address must be 8 bit\n");
        return msg;
    }

    // extract value
    unsigned int value;
    arg = cli_next_hex(&value, arg);
    if (arg == NULL) {
        ceda_string_cpy(msg, USER_BAD_ARG_STR "missing value\n");
        return msg;
    }
    if (value >= 0x100) {
        ceda_string_cpy(msg, "value must be 8 bit\n");
        return msg;
    }

    bus_io_out((ceda_ioaddr_t)address, (zuint8)value);

    ceda_string_delete(msg);
    return NULL;
}

/*
    A cli_command_handler_t is a command line handler.
    It takes a pointer to the line buffer.
    It returns a pointer to a ceda_string_t,
    which is the response to the command.
    Caller takes ownership of the returned ceda_string_t,
    and must free() it when done.
    NULL can be returned for an empty message.
    An empty message is treated as a generic "success" condition.
*/
typedef ceda_string_t *(*cli_command_handler_t)(const char *);

// TODO(giomba): possibly to be extended
typedef struct cli_command {
    const char *command;
    const char *help;
    cli_command_handler_t handler;
} cli_command;

static ceda_string_t *cli_help(const char *arg);
static const cli_command cli_commands[] = {
    {"dis", "disassembly binary data", cli_dis},
    {"break", "set or show cpu breakpoints", cli_break},
    {"delete", "delete cpu breakpoint", cli_delete},
    {"pause", "pause cpu execution", cli_pause},
    {"continue", "continue cpu execution", cli_continue},
    {"reg", "show cpu registers", cli_reg},
    {"step", "step one instruction", cli_step},
    {"goto", "override cpu program counter", cli_goto},
    {"read", "read from memory", cli_read},
    {"write", "write to memory", cli_write},
    {"in", "read from io", cli_in},
    {"out", "write to io", cli_out},
    {"load", "load binary from file", cli_load},
    {"save", "save memory dump to file", cli_save},
    {"quit", "quit the emulator", cli_quit},
    {"help", "show this help", cli_help},
};

/**
 * @brief Parse the command line and execute command.
 *
 * Find the first word in the command line, and try to execute it as a
 * command.
 *
 * @param line pointer to ceda_string_t string representing the command line
 */
static void cli_handle_line(ceda_string_t *line) {
    // TODO(giomba): this is a "memory leak",
    // because nobody is going to free this last_line ceda_string_t.
    static ceda_string_t *last_line = NULL;
    if (last_line == NULL)
        last_line = ceda_string_new(0);

    // size == 0 => reuse last command line
    if (ceda_string_len(line) == 0) {
        line = last_line;
    }

    // last command line is empty => do nothing
    if (ceda_string_len(line) == 0) {
        cli_send_string(USER_PROMPT_STR);
        return;
    }

    // search for first word
    char word[LINE_BUFFER_SIZE];
    cli_next_word(word, ceda_string_data(line), LINE_BUFFER_SIZE);

    // look for `word` in the command set
    for (size_t i = 0; i < ARRAY_SIZE(cli_commands); ++i) {
        const cli_command *const c = &cli_commands[i];

        // command found
        if (strcmp(c->command, word) == 0) {
            // save line for next time
            if (last_line != line)
                ceda_string_cpy(last_line, ceda_string_data(line));
            ceda_string_t *msg = c->handler(ceda_string_data(line));
            if (msg != NULL) {
                cli_send_string(ceda_string_data(msg));
                ceda_string_delete(msg);
            }
            cli_send_string(USER_PROMPT_STR);
            return;
        }
    }

    // if no command has been found in the line
    ceda_string_cpy(last_line, "");
    cli_send_string("command not found\n");
    cli_send_string(USER_PROMPT_STR);
}

/**
 * @brief Split incoming raw data into lines separed by '\n'.
 *
 * This function also discards any '\r' and '\n' from the incoming stream,
 * and produce a nice null-terminated C string for each incoming line.
 *
 * Then, lines are parsed as commands.
 *
 * @param buffer Pointer to raw data buffer.
 * @param size Lenght of raw data.
 */
static void cli_handle_incoming_data(const char *buffer, size_t size) {
    static char line[LINE_BUFFER_SIZE] = {};
    static size_t count = 0;

    for (size_t i = 0; i < size; ++i) {
        const char c = buffer[i];

        // discard cr
        if (c == '\r')
            continue;

        // new line: handle what has been read
        if (c == '\n' || count == LINE_BUFFER_SIZE - 1) {
            line[count] = '\0';
            ceda_string_t *line_string = ceda_string_new(0);
            ceda_string_cpy(line_string, line);
            cli_handle_line(line_string);
            ceda_string_delete(line_string);
            count = 0;
            continue;
        }

        line[count++] = c;
    }
}

static ceda_string_t *cli_help(const char *arg) {
    (void)arg;

    ceda_string_t *msg = ceda_string_new(0);

    for (size_t i = 0; i < ARRAY_SIZE(cli_commands); ++i) {
        const cli_command *const c = &cli_commands[i];
        ceda_string_printf(msg, "\t%s\n\t\t%s\n\n", c->command, c->help);
    }

    return msg;
}

static void cli_start(void) {
    if (!initialized)
        return;
}

static void cli_poll(void) {
    last_update = time_now_us();

    if (!initialized)
        return;

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // if no client is connected, select() for the accept()
    if (connfd == -1) {
        fd_set accept_set;
        FD_ZERO(&accept_set);
        FD_SET(sockfd, &accept_set);
        int ret = select(sockfd + 1, &accept_set, NULL, NULL, &timeout);
        if (ret == -1) {
            LOG_ERR("error during select while accepting new client: %s\n",
                    strerror(errno));
            return;
        }
        if (ret == 0) {
            // just a timeout
            return;
        }

        LOG_DEBUG("accept cli client\n");
        connfd = accept(sockfd, NULL, NULL);
        cli_send_string(USER_PROMPT_STR);

        // a client is connected, select() for read() or write()
        // this is reasonable because we handle only one client
    } else {
        int ret = -1;
        fd_set read_set;
        fd_set write_set;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(connfd, &read_set);
        FD_SET(connfd, &write_set);

        ret = select(connfd + 1, &read_set, &write_set, NULL, &timeout);
        if (ret == -1) {
            LOG_ERR("select error while reading from client: %s\n",
                    strerror(errno));
            close(connfd);
            connfd = -1;
            return;
        }
        if (ret == 0) {
            // just a timeout
            return;
        }

        // check file descriptors ready for read
        if (FD_ISSET(connfd, &read_set)) {
            char buffer[256];
            ssize_t ret = recv(connfd, buffer, 256 - 1, 0);
            if (ret == -1) {
                LOG_ERR("recv error while reading from client: %s\n",
                        strerror(errno));
                close(connfd);
                connfd = -1;
                return;
            }
            if (ret == 0) {
                // client disconnection
                close(connfd);
                connfd = -1;
                return;
            }
            // data available
            cli_handle_incoming_data(buffer, (size_t)ret);
        }

        // check file descriptors ready for write
        if (FD_ISSET(connfd, &write_set)) {
            while (!FIFO_ISEMPTY(&tx_fifo)) {
                ceda_string_t *message = FIFO_POP(&tx_fifo);

                ssize_t ret = send(connfd, ceda_string_data(message),
                                   strlen(ceda_string_data(message)), 0);
                ceda_string_delete(message);

                if (ret == -1) {
                    LOG_ERR("send error while writing to client: %s\n",
                            strerror(errno));
                    close(connfd);
                    connfd = -1;
                    return;
                }
            }
        }
    }
}

static us_interval_t cli_remaining(void) {
    const us_time_t next_update = last_update + UPDATE_INTERVAL;
    const us_time_t now = time_now_us();
    const us_interval_t diff = next_update - now;
    return diff;
}

void cli_cleanup(void) {
    if (!initialized)
        return;

    if (connfd != -1)
        close(connfd);
    if (sockfd != -1)
        close(sockfd);
}

void cli_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));
    mod->init = cli_init;
    mod->start = cli_start;
    mod->poll = cli_poll;
    mod->remaining = cli_remaining;
    mod->cleanup = cli_cleanup;

    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        LOG_WARN("unable to socket(): %s\n", strerror(errno));
        return;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(CLI_PORT);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){true},
                   sizeof(int)) != 0) {
        LOG_WARN("unable to setsockopt(): %s\n", strerror(errno));
        return;
    }

    if (bind(sockfd, (const struct sockaddr *)&server_addr,
             sizeof(server_addr)) != 0) {
        LOG_WARN("unable to bind(): %s\n", strerror(errno));
        return;
    }

    if (listen(sockfd, 1) != 0) {
        LOG_WARN("unable to listen(): %s\n", strerror(errno));
        return;
    }

    FIFO_INIT(&tx_fifo);

    LOG_INFO("cli ok\n");
    initialized = true;
}

#ifdef CEDA_TEST

#include <criterion/criterion.h>

struct test {
    bool input; // true => is user input line,
                // false => is emulator expected response
    const char *text;
};

static void cli_test_setup(void) {
    FIFO_INIT(&tx_fifo);
}

static void run_tests(struct test *tests, size_t n) {
    // static bool prompt = false;

    for (size_t i = 0; i < n; ++i) {
        struct test *tst = &tests[i];

#if 0
        LOG_DEBUG("test %s %s\n", tst->input ? "=>" : "<=", tst->text);
#endif

        if (tst->input) {
            ceda_string_t *text = ceda_string_new(0);
            ceda_string_cpy(text, tst->text);
            cli_handle_line(text);
            ceda_string_delete(text);
        } else {
            const char *expected = tst->text ? tst->text : USER_PROMPT_STR;
            cr_assert(!FIFO_ISEMPTY(&tx_fifo));
            ceda_string_t *message = FIFO_POP(&tx_fifo);
            cr_assert_str_eq(ceda_string_data(message), expected);
            ceda_string_delete(message);
        }
    }
}

Test(cli, break, .init = cli_test_setup) {
    /* clang-format off */
    struct test tests[] = {
        {true,  "break"},
        {false, "no breakpoint set\n"},
        {false, USER_PROMPT_STR},
        {true,  "break c000"},
        {false, USER_PROMPT_STR},
        {true,  "break"},
        {false, "0\tc000\n"},
        {false, USER_PROMPT_STR},
        {true,  "break c030"},
        {false, USER_PROMPT_STR},
        {true,  "break"},
        {false, "0\tc000\n1\tc030\n"},
        {false, USER_PROMPT_STR},
    };
    /* clang-format on */
    run_tests(tests, ARRAY_SIZE(tests));
}

Test(cli, delete, .init = cli_test_setup) {
    /* clang-format off */
    struct test tests[] = {
        {true,  "break c000"},
        {false, USER_PROMPT_STR},
        {true,  "break c030"},
        {false, USER_PROMPT_STR},
        {true,  "delete"},
        {false, USER_BAD_ARG_STR "missing delete target\n"},
        {false, USER_PROMPT_STR},
        {true,  "delete breakpoint"},
        {false, USER_BAD_ARG_STR "missing index\n"},
        {false, USER_PROMPT_STR},
        {true,  "delete brekpoi 1"},
        {false, USER_BAD_ARG_STR "unknown delete target\n"},
        {false, USER_PROMPT_STR},
        {true,  "delete breakpoint -1"},
        {false, "can't delete breakpoint\n"},
        {false, USER_PROMPT_STR},
        {true,  "delete breakpoint xx"},
        {false, USER_BAD_ARG_STR "bad index format\n"},
        {false, USER_PROMPT_STR},
        {true,  "delete breakpoint 0"},
        {false, USER_PROMPT_STR},
        {true,  "break"},
        {false, "1\tc030\n"},
        {false, USER_PROMPT_STR},
    };
    /* clang-format on */
    run_tests(tests, ARRAY_SIZE(tests));
}

Test(cli, next_word) {
    const char *prompt = "   The quick  brown   fox";
    const char *words[] = {"The", "quick", "brown", "fox"};

    // check tokenize capability
    char word[LINE_BUFFER_SIZE];
    for (size_t i = 0; i < ARRAY_SIZE(words); ++i) {
        prompt = cli_next_word(word, prompt, LINE_BUFFER_SIZE);
        cr_assert_str_eq(word, words[i]);
    }

    // no more words
    prompt = cli_next_word(word, prompt, LINE_BUFFER_SIZE);
    cr_assert_eq(prompt, NULL);

    // check length constraints
    const size_t constraint = 6;
    cli_next_word(word, "supercalifragilisticexpialidocious", constraint);
    cr_assert_str_eq(word, "super");
}

Test(cli, next_hex) {
    const char *prompt = " 12 ab xx 77 ";
    const unsigned int values[] = {0x12, 0xab};

    unsigned int value = 0;
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
        prompt = cli_next_hex(&value, prompt);
        cr_assert_eq(value, values[i]);
    }

    prompt = cli_next_hex(&value, prompt);
    cr_assert_eq(prompt, NULL);
}

Test(cli, next_int) {
    const char *prompt = "12 432 7a a7";
    const unsigned int values[] = {12, 432, 7};

    unsigned int value;
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i) {
        prompt = cli_next_int(&value, prompt);
        cr_assert_eq(value, values[i]);
    }

    prompt = cli_next_int(&value, prompt);
    cr_assert_eq(prompt, NULL);
}

#endif
