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

#define USER_PROMPT_STR     "> "
#define COMMAND_BUFFER_SIZE 128
#define HELP_BUFFER_SIZE    4096

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

static char *cli_quit(void *arg) {
    (void)arg;

    quit = true;

    return NULL;
}

// TODO - to be extended
typedef char *(*cli_command_handler_t)(void *);
typedef struct cli_command {
    const char *command;
    const char *help;
    cli_command_handler_t handler;
} cli_command;

static char *cli_help(void *);
static const cli_command cli_commands[] = {
    {"quit", "quit the emulator", cli_quit},
    {"help", "show this help", cli_help},
};

static void cli_handle_command(const char *buffer, size_t size) {
    // size == 0 => reuse last command
    if (size == 0) {
        cli_send_string(USER_PROMPT_STR);
        return; // TODO
    }

    // search for a potentially good command
    for (size_t i = 0; i < ARRAY_SIZE(cli_commands); ++i) {
        const cli_command *const c = &cli_commands[i];
        if (strncmp(c->command, buffer, MIN(size, strlen(c->command))) == 0) {
            const char *m = c->handler(NULL);
            if (m != NULL)
                cli_send_string(m);
            cli_send_string(USER_PROMPT_STR);
            return;
        }
    }
    cli_send_string("command not found\n");
    cli_send_string(USER_PROMPT_STR);
}

static void cli_handle_incoming_data(const char *buffer, size_t size) {
    static char command[COMMAND_BUFFER_SIZE] = {0};
    static size_t count = 0;

    for (size_t i = 0; i < size; ++i) {
        const char c = buffer[i];

        // discard cr
        if (c == '\r')
            continue;

        // new line: handle what has been read
        if (c == '\n' || count == COMMAND_BUFFER_SIZE) {
            cli_handle_command(command, count);
            count = 0;
            continue;
        }

        command[count++] = c;
    }
}

static char *cli_help(void *arg) {
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
