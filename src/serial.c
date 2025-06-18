#include "serial.h"

#include "fifo.h"
#include "sio2.h"
#include "time.h"

#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define LOG_LEVEL LOG_LVL_INFO
#include "log.h"

#define SERIAL_TCP_PORT            (0xCEDB)
#define SERIAL_NETWORK_BUFFER_SIZE 64U

DECLARE_FIFO_TYPE(char, SerialFifo, 64);
static int sockfd = -1;
static int connfd = -1;
static SerialFifo tx_fifo;
static SerialFifo rx_fifo;

static bool serial_getChar(uint8_t *c) {
    if (FIFO_ISEMPTY(&rx_fifo))
        return false;

    *c = (uint8_t)FIFO_POP(&rx_fifo);
    return true;
}

static bool serial_putChar(uint8_t c) {
    if (FIFO_ISFULL(&tx_fifo))
        return false;

    LOG_DEBUG("serial: transmitting: %02x (%c)\n", (unsigned int)c,
              isprint(c) ? c : ' ');

    FIFO_PUSH(&tx_fifo, (char)c);
    return true;
}

static void serial_poll(void) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (sockfd < 0)
        return;

    if (connfd == -1) {
        fd_set accept_set;
        FD_ZERO(&accept_set);
        FD_SET(sockfd, &accept_set);
        int ret = select(sockfd + 1, &accept_set, NULL, NULL, &timeout);
        if (ret == -1) {
            LOG_ERR(
                "serial: error during select while accepting new client: %s\n",
                strerror(errno));
            return;
        }
        if (ret == 0) // timeout
            return;

        connfd = accept(sockfd, NULL, NULL);
        if (connfd != -1) {
            LOG_INFO("serial: accept client\n");
        }
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
            LOG_ERR("serial: select error while reading from client: %s\n",
                    strerror(errno));
            close(connfd);
            connfd = -1;
            return;
        }
        if (ret == 0) // timeout
            return;

        // check file descriptors ready for read
        if (FD_ISSET(connfd, &read_set)) {
            char buffer[SERIAL_NETWORK_BUFFER_SIZE];
            const size_t to_receive = MIN((size_t)SERIAL_NETWORK_BUFFER_SIZE,
                                          (size_t)FIFO_FREE(&rx_fifo));
            if (to_receive > 0) {
                ssize_t ret = recv(connfd, buffer, to_receive, 0);
                if (ret == -1) {
                    LOG_ERR(
                        "serial: recv error while reading from client: %s\n",
                        strerror(errno));
                    LOG_ERR("serial: connection reset\n");
                    close(connfd);
                    connfd = -1;
                    return;
                }
                if (ret == 0) {
                    // client disconnection
                    close(connfd);
                    connfd = -1;
                    LOG_INFO("serial: client disconnected\n");
                    return;
                }
                // data available
                for (ssize_t i = 0; i < ret && !FIFO_ISFULL(&rx_fifo); ++i)
                    FIFO_PUSH(&rx_fifo, buffer[i]);
            }
        }

        // check file descriptors ready for write
        if (FD_ISSET(connfd, &write_set)) {
            char buffer[SERIAL_NETWORK_BUFFER_SIZE];
            size_t n = 0;
            while (n < SERIAL_NETWORK_BUFFER_SIZE && !FIFO_ISEMPTY(&tx_fifo))
                buffer[n++] = FIFO_POP(&tx_fifo);
            ssize_t ret = send(connfd, buffer, n, 0);

            if (ret == -1) {
                LOG_ERR("serial: send error while writing to client: %s\n",
                        strerror(errno));
                LOG_ERR("serial: connection reset\n");
                close(connfd);
                connfd = -1;
                return;
            }
        }
    }
}

bool serial_open(uint16_t port) {
    if (sockfd >= 0) {
        LOG_INFO("serial: port already open\n");
        return false;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        LOG_ERR("serial: unable to socket(): %s\n", strerror(errno));
        return false;
    }

    if (port == 0)
        port = SERIAL_TCP_PORT;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){true},
                   sizeof(int)) != 0) {
        LOG_ERR("serial: unable to setsockopt(): %s\n", strerror(errno));
        return false;
    }

    if (bind(sockfd, (const struct sockaddr *)&server_addr,
             sizeof(server_addr)) != 0) {
        LOG_ERR("serial: unable to bind(): %s\n", strerror(errno));
        return false;
    }

    if (listen(sockfd, 1) != 0) {
        LOG_ERR("serial: unable to listen(): %s\n", strerror(errno));
        return false;
    }

    FIFO_INIT(&tx_fifo);
    FIFO_INIT(&rx_fifo);

    sio2_attachPeripheral(SIO_CHANNEL_A, serial_getChar, serial_putChar);

    LOG_INFO("serial: open ok\n");
    return true;
}

void serial_close(void) {
    sio2_detachPeripheral(SIO_CHANNEL_A);

    if (connfd != -1)
        close(connfd);

    if (sockfd != -1)
        close(sockfd);

    LOG_INFO("serial: close ok\n");
}

static void serial_cleanup(void) {
    serial_close();
}

void serial_init(CEDAModule *mod) {
    memset(mod, 0, sizeof(*mod));
    mod->init = serial_init;
    mod->poll = serial_poll;
    mod->cleanup = serial_cleanup;
}
