#include "cli.h"

#include "3rd/disassembler.h"
#include "3rd/fifo.h"
#include "bus.h"
#include "cpu.h"
#include "macro.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#define LOG_FORMAT LOG_FMT_VERBOSE
#define LOG_LEVEL  LOG_LVL_DEBUG
#include "log.h"

#define CLI_PORT 0xceda

#define USER_PROMPT_STR   "> "
#define LINE_BUFFER_SIZE  128  // small line-like stuff
#define BLOCK_BUFFER_SIZE 4096 // big page-like stuff

#define USER_BAD_ARG_STR       "bad argument\n"
#define USER_NO_SPACE_LEFT_STR "no space left\n"

static bool initialized = false;
static bool quit = false;

static int sockfd = -1;
static int connfd = -1;

DECLARE_FIFO_TYPE(char *, TxFifo, 8);
static TxFifo tx_fifo;

void cli_init(void) {
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

void cli_start(void) {
    if (!initialized)
        return;
}

static void cli_send_string(const char *str) {
    const size_t alloc_size = strlen(str) + 1;

    char *m = malloc(alloc_size);
    strncpy(m, str, alloc_size);

    FIFO_PUSH(&tx_fifo, m);
}

static char *cli_quit(const char *arg) {
    (void)arg;

    quit = true;

    return NULL;
}

static char *cli_pause(const char *arg) {
    (void)arg;
    cpu_pause(true);
    return NULL;
}

static char *cli_continue(const char *arg) {
    (void)arg;
    cpu_step(); // possibly step past the breakpoint
    cpu_pause(false);
    return NULL;
}

static char *cli_reg(const char *arg) {
    (void)arg;
    CpuRegs regs;
    cpu_reg(&regs);

    // disassemble current pc
    char _dis[LINE_BUFFER_SIZE];
    uint8_t blob[16];
    bus_mem_readsome(NULL, blob, regs.pc, 16);
    disassemble(blob, regs.pc, _dis, LINE_BUFFER_SIZE);
    const char *dis = _dis;
    while (*dis == ' ')
        ++dis;

    char *m = malloc(BLOCK_BUFFER_SIZE);

    /* clang-format off */
    /* don't pretend miracles from the formatter */
    snprintf(
        m, BLOCK_BUFFER_SIZE,

        // string format
        " %s\n"
        " PC   SP   AF   BC   DE   HL   AF'  BC'  DE'  HL' IX IY\n"
        "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %02x %02x\n",

        // varargs
        dis,
        regs.pc, regs.sp, regs.fg.af, regs.fg.bc, regs.fg.de, regs.fg.hl,
        regs.bg.af, regs.bg.bc, regs.bg.de, regs.bg.hl, regs.ix, regs.iy);
    /* clang-format on */

    return m;
}

static char *cli_step(const char *arg) {
    cpu_step();
    return cli_reg(arg);
}

static char *cli_break(const char *arg) {
    // skip first arg
    while (*arg != ' ' && *arg != '\0') {
        ++arg;
    }

    // no arg => show current breakpoints
    if (*arg == '\0') {
        char *m = malloc(LINE_BUFFER_SIZE);
        strncpy(m, "no breakpoint set\n", LINE_BUFFER_SIZE);

        CpuBreakpoint *breakpoints;
        const size_t n = cpu_getBreakpoints(&breakpoints);

        int w = 0;
        for (size_t i = 0; i < n && w < LINE_BUFFER_SIZE - 1; ++i) {
            if (!breakpoints[i].valid)
                continue;
            w += snprintf(m + w, LINE_BUFFER_SIZE - w, "%lu\t%04x\n", i,
                          breakpoints[i].address);
        }
        return m;
    }

    // extract address
    ++arg;
    unsigned int _address;
    int n = sscanf(arg, "%04x", &_address);
    if (n != 1) {
        char *m = malloc(LINE_BUFFER_SIZE);
        strncpy(m, USER_BAD_ARG_STR, LINE_BUFFER_SIZE);
        return m;
    }

    zuint16 address;
    address = _address;

    // actually set breakpoint
    bool r = cpu_addBreakpoint(address);

    if (!r) {
        char *m = malloc(LINE_BUFFER_SIZE);
        strncpy(m, USER_NO_SPACE_LEFT_STR, LINE_BUFFER_SIZE);
        return m;
    }

    return NULL;
}

static char *cli_delete(const char *arg) {
    char *m = malloc(LINE_BUFFER_SIZE);
    strncpy(m, "", LINE_BUFFER_SIZE);

    // skip first arg
    while (*arg != ' ' && *arg != '\0') {
        ++arg;
    }

    // missing what
    if (*arg == '\0') {
        strncpy(m, USER_BAD_ARG_STR "missing delete target\n",
                LINE_BUFFER_SIZE);
        return m;
    }

    // extract what to delete (breakpoint, watchpoint, ...)
    ++arg;
    char what[LINE_BUFFER_SIZE];
    size_t n = 0;
    while (*arg != ' ' && *arg != '\0') {
        what[n++] = *arg++;
    }
    what[n] = '\0';

    // missing index
    if (*arg == '\0') {
        strncpy(m, USER_BAD_ARG_STR "missing index\n", LINE_BUFFER_SIZE);
        return m;
    }

    // extract index
    ++arg;
    unsigned int index;
    int r = sscanf(arg, "%u", &index);
    if (r != 1) {
        strncpy(m, USER_BAD_ARG_STR "bad index format\n", LINE_BUFFER_SIZE);
        return m;
    }

    // actually delete something
    if (strcmp(what, "breakpoint") == 0) {
        if (!cpu_deleteBreakpoint(index)) {
            strncpy(m, "can't delete breakpoint\n", LINE_BUFFER_SIZE);
            return m;
        }
    } else if (strcmp(what, "watchpoint") == 0) {
        // TODO
    } else {
        strncpy(m, USER_BAD_ARG_STR "unknown delete target\n",
                LINE_BUFFER_SIZE);
        return m;
    }

    // all ok
    free(m);
    return NULL;
}

static char *cli_read(const char *arg) {
    char *m = malloc(BLOCK_BUFFER_SIZE);

    // skip first arg
    while (*arg != ' ' && *arg != '\0') {
        ++arg;
    }

    // missing address
    if (*arg == '\0') {
        strncpy(m, USER_BAD_ARG_STR "missing address\n", BLOCK_BUFFER_SIZE);
        return m;
    }

    // extract address
    ++arg;
    unsigned int _address;
    int r = sscanf(arg, "%x", &_address);
    if (r != 1) {
        strncpy(m, USER_BAD_ARG_STR "bad address format\n", BLOCK_BUFFER_SIZE);
        return m;
    }

    // perform read
    zuint16 address = _address;
    const size_t BLOB_SIZE = 8 * 16;
    char blob[BLOB_SIZE];
    bus_mem_readsome(NULL, blob, address, BLOB_SIZE);

    // print nice hexdump
    int n = 0;
    char ascii[16 + 1] = {0};
    for (unsigned int i = 0; i < BLOB_SIZE && n < BLOCK_BUFFER_SIZE - 1; ++i) {
        const char c = blob[i];

        if (i % 16 == 0) {
            n += snprintf(m + n, BLOCK_BUFFER_SIZE - n, "%04x\t", address + i);
        }

        n += snprintf(m + n, BLOCK_BUFFER_SIZE - n, "%02x ",
                      ((unsigned int)(c)) & 0xff);
        ascii[i % 16] = isprint(c) ? c : '.';

        if (i % 16 == 7) {
            n += snprintf(m + n, BLOCK_BUFFER_SIZE - n, " ");
        }

        if (i % 16 == 15) {
            n += snprintf(m + n, BLOCK_BUFFER_SIZE - n, "\t%s\n", ascii);
        }
    }

    return m;
}

static char *cli_write(const char *arg) {
    char *m = malloc(LINE_BUFFER_SIZE);

    // skip first arg
    while (*arg != ' ' && *arg != '\0') {
        ++arg;
    }

    // missing address
    if (*arg == '\0') {
        strncpy(m, USER_BAD_ARG_STR "missing address\n", LINE_BUFFER_SIZE);
        return m;
    }

    // extract address
    ++arg;
    unsigned int _address;
    int r = sscanf(arg, "%x", &_address);
    if (r != 1 || _address >= 0x10000) {
        strncpy(m, USER_BAD_ARG_STR "bad address format\n", LINE_BUFFER_SIZE);
        return m;
    }

    // next arg
    while (*arg != ' ' && *arg != '\0') {
        ++arg;
    }
    // skip space
    if (*arg == ' ')
        ++arg;

    // missing value
    if (*arg == '\0') {
        strncpy(m, USER_BAD_ARG_STR "missing value\n", LINE_BUFFER_SIZE);
        return m;
    }

    zuint16 address = _address;
    for (unsigned int i = 0;; ++i) {
        // atoi value
        unsigned int value;
        int r = sscanf(arg, "%x", &value);
        if (r != 1 || value >= 0x100) {
            strncpy(m, USER_BAD_ARG_STR "bad value format\n", LINE_BUFFER_SIZE);
            return m;
        }
        bus_mem_write(NULL, address + i, value);

        // next value
        while (*arg != ' ' && *arg != '\0') {
            ++arg;
        }
        // skip space
        if (*arg == ' ')
            ++arg;

        LOG_DEBUG("next arg = %d\n", ((int)(*arg)) & 0xff);
        // no more values
        if (*arg == '\0')
            break;
    }

    free(m);
    return NULL;
}

/*
    A cli_command_handler_t is a command line handler.
    It takes a pointer to the line buffer.
    It returns a pointer to a null-terminated C string,
    which is the response to the command (NULL if none).
    Caller takes ownership of the returned string,
    and must free() it when done.
*/
typedef char *(*cli_command_handler_t)(const char *);

// TODO - to be extended
typedef struct cli_command {
    const char *command;
    const char *help;
    cli_command_handler_t handler;
} cli_command;

static char *cli_help(const char *);
static const cli_command cli_commands[] = {
    {"break", "set or show cpu breakpoints", cli_break},
    {"delete", "delete cpu breakpoint", cli_delete},
    {"pause", "pause cpu execution", cli_pause},
    {"continue", "continue cpu execution", cli_continue},
    {"reg", "show cpu registers", cli_reg},
    {"step", "step one instruction", cli_step},
    {"read", "read from memory", cli_read},
    {"write", "write to memory", cli_write},
    {"quit", "quit the emulator", cli_quit},
    {"help", "show this help", cli_help},
};

/**
 * @brief Parse the command line and execute command.
 *
 * Find the first word in the command line, and try to execute it as a
 * command.
 *
 * @param line null-terminated C string representing the command line
 */
static void cli_handle_line(const char *line) {
    static char last_line[LINE_BUFFER_SIZE] = {0};

    // size == 0 => reuse last command line
    if (strlen(line) == 0) {
        line = last_line;
    }

    // last command line is empty => do nothing
    if (strlen(line) == 0) {
        cli_send_string(USER_PROMPT_STR);
        return;
    }

    // search for first word
    char word[LINE_BUFFER_SIZE];
    for (size_t i = 0; i < LINE_BUFFER_SIZE; ++i) {
        const char c = line[i];
        word[i] = c;
        if (c == ' ' || c == '\0') {
            word[i] = '\0';
            break;
        }
    }

    // look for `word` in the command set
    for (size_t i = 0; i < ARRAY_SIZE(cli_commands); ++i) {
        const cli_command *const c = &cli_commands[i];

        // command found
        if (strcmp(c->command, word) == 0) {
            strncpy(last_line, line,
                    LINE_BUFFER_SIZE); // save line for next time
            char *m = c->handler(line);
            if (m != NULL) {
                cli_send_string(m);
                free(m);
            }
            cli_send_string(USER_PROMPT_STR);
            return;
        }
    }

    // if no command has been found in the line
    strncpy(last_line, "", LINE_BUFFER_SIZE);
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
    static char line[LINE_BUFFER_SIZE] = {0};
    static size_t count = 0;

    for (size_t i = 0; i < size; ++i) {
        const char c = buffer[i];

        // discard cr
        if (c == '\r')
            continue;

        // new line: handle what has been read
        if (c == '\n' || count == LINE_BUFFER_SIZE - 1) {
            line[count] = '\0';
            cli_handle_line(line);
            count = 0;
            continue;
        }

        line[count++] = c;
    }
}

static char *cli_help(const char *arg) {
    (void)arg;

    char *m = malloc(BLOCK_BUFFER_SIZE);
    if (m == NULL) {
        LOG_ERR("out of memory\n");
        return NULL;
    }

    size_t n = 0;
    for (size_t i = 0;
         i < ARRAY_SIZE(cli_commands) && n < BLOCK_BUFFER_SIZE - 1; ++i) {
        const cli_command *const c = &cli_commands[i];
        n += snprintf(m + n, BLOCK_BUFFER_SIZE - n, "\t%s\n\t\t%s\n\n",
                      c->command, c->help);
    }

    return m;
}

void cli_poll(void) {
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
        int rv = select(sockfd + 1, &accept_set, NULL, NULL, &timeout);
        if (rv == -1) {
            LOG_ERR("error during select while accepting new client: %s\n",
                    strerror(errno));
            return;
        } else if (rv == 0) {
            // just a timeout
            return;
        } else {
            LOG_DEBUG("accept cli client\n");
            connfd = accept(sockfd, NULL, NULL);
            cli_send_string(USER_PROMPT_STR);
        }
        // a client is connected, select() for read() or write()
        // this is reasonable because we handle only one client
    } else {
        int rv = -1;
        fd_set read_set, write_set;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(connfd, &read_set);
        FD_SET(connfd, &write_set);

        rv = select(connfd + 1, &read_set, &write_set, NULL, &timeout);
        if (rv == -1) {
            LOG_ERR("select error while reading from client: %s\n",
                    strerror(errno));
            close(connfd);
            connfd = -1;
            return;
        } else if (rv == 0) {
            // just a timeout
            return;
        }

        // check file descriptors ready for read
        if (FD_ISSET(connfd, &read_set)) {
            char buffer[256];
            ssize_t rv = recv(connfd, buffer, 256 - 1, 0);
            if (rv == -1) {
                LOG_ERR("recv error while reading from client: %s\n",
                        strerror(errno));
                close(connfd);
                connfd = -1;
                return;
            } else if (rv == 0) {
                // client disconnection
                close(connfd);
                connfd = -1;
                return;
            } else {
                // data available
                cli_handle_incoming_data(buffer, rv);
            }
        }

        // check file descriptors ready for write
        if (FD_ISSET(connfd, &write_set)) {
            while (!FIFO_ISEMPTY(&tx_fifo)) {
                char *m = FIFO_POP(&tx_fifo);

                int rv = send(connfd, m, strlen(m), 0);
                free(m);

                if (rv == -1) {
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

bool cli_isQuit(void) {
    return quit;
}

void cli_cleanup(void) {
    if (!initialized)
        return;

    if (connfd != -1)
        close(connfd);
    if (sockfd != -1)
        close(sockfd);
}
