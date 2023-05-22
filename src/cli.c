#include "cli.h"

#include "3rd/fifo.h"
#include "macro.h"

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

#define USER_PROMPT_STR "> "
#define USER_PROMPT_LEN 2

static bool initialized = false;
static bool quit = false;

static int sockfd = -1;
static int connfd = -1;

DECLARE_FIFO_TYPE(void *, TxFifo, 8);
TxFifo tx_fifo;
struct message {
    size_t len;
};

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

static void *cli_quit(void *arg) {
    (void)arg;

    quit = true;

    return NULL;
}

// TODO - to be extended
typedef void *(*cli_command_handler_t)(void *);
typedef struct cli_command {
    const char *command;
    const char *help;
    cli_command_handler_t handler;
} cli_command;

static void *cli_help(void *);
static const cli_command cli_commands[] = {
    {"quit", "quit the emulator", cli_quit},
    {"help", "show this help", cli_help},
};

static void cli_handle_command(char *buffer, size_t size) {
    for (size_t i = 0; i < ARRAY_SIZE(cli_commands); ++i) {
        const cli_command *const c = &cli_commands[i];
        if (strncmp(c->command, buffer, MIN(size, strlen(c->command))) == 0) {
            void *m = c->handler(NULL);
            if (m != NULL)
                FIFO_PUSH(&tx_fifo, m);
            return;
        }
    }
}

static void *cli_help(void *arg) {
    (void)arg;
#define BUFFER_SIZE 4096

    char *buffer = malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        LOG_ERR("out of memory\n");
        return NULL;
    }

    size_t n = 0;
    struct message *m = (struct message *)buffer;
    char *payload = buffer + sizeof(*m);

    for (size_t i = 0; i < ARRAY_SIZE(cli_commands); ++i) {
        const cli_command *const c = &cli_commands[i];
        n += snprintf(payload + n, BUFFER_SIZE - n, "\t%s\n\t\t%s\n\n",
                      c->command, c->help);
    }

    m->len = n;
    return buffer;
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
            send(connfd, USER_PROMPT_STR, USER_PROMPT_LEN, 0);
        }
        // a client is connected, select() for read() or write()
        // this is reasonable because we handle only one client (at the moment)
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
            LOG_DEBUG("timeout\n");
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
                cli_handle_command(buffer, (size_t)rv);
            }
        }

        // check file descriptors ready for write
        if (FD_ISSET(connfd, &write_set)) {
            if (!FIFO_ISEMPTY(&tx_fifo)) {
                struct message *m = FIFO_POP(&tx_fifo);
                void *payload = &m[1];

                int rv0 = send(connfd, payload, m->len, 0);
                free(m);
                int rv1 = send(connfd, USER_PROMPT_STR, USER_PROMPT_LEN, 0);

                if (rv0 == -1 || rv1 == -1) {
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
