#include "cli.h"

#include "3rd/fifo.h"
#include "macro.h"

#include <assert.h>
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

#define USER_PROMPT_STR  "> "
#define LINE_BUFFER_SIZE 128
#define HELP_BUFFER_SIZE 4096

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
    strcpy(m, str);

    FIFO_PUSH(&tx_fifo, m);
}

static char *cli_quit(const char *arg) {
    (void)arg;

    quit = true;

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
            strcpy(last_line, line); // save line for next time
            const char *m = c->handler(NULL);
            if (m != NULL)
                cli_send_string(m);
            cli_send_string(USER_PROMPT_STR);
            return;
        }
    }

    // if no command has been found in the line
    strcpy(last_line, "");
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

    char *m = malloc(HELP_BUFFER_SIZE);
    if (m == NULL) {
        LOG_ERR("out of memory\n");
        return NULL;
    }

    size_t n = 0;
    for (size_t i = 0; i < ARRAY_SIZE(cli_commands); ++i) {
        const cli_command *const c = &cli_commands[i];
        n += snprintf(m + n, HELP_BUFFER_SIZE - n, "\t%s\n\t\t%s\n\n",
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
