#include "sio2.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "bus.h"
#include "fifo.h"
#include "keyboard.h"
#include "macro.h"

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

typedef bool (*sio_channel_try_read_t)(uint8_t *c);
typedef bool (*sio_channel_try_write_t)(uint8_t c);

DECLARE_FIFO_TYPE(uint8_t, SIOFIFO, (3 + 1));

typedef struct SIOChannel {
    uint8_t reg_index;    //< pointer to indexed internal register
    uint8_t read_regs[3]; //< read registers

    SIOFIFO rx_fifo; //< receiver FIFO buffer
    SIOFIFO tx_fifo; //< transmitter FIFO buffer

    bool rx_enabled;     //< enable serial RX
    bool tx_enabled;     //< enable serial TX
    bool rx_int_enabled; //< enable interrupts on RX
    bool tx_int_enabled; //< enable interrupts on TX

    // Get character from attached serial peripheral (callback).
    // If NULL, no peripheral is phisically attached.
    sio_channel_try_read_t getc;
    // Put character to attached serial peripheral (callback).
    // If NULL, no peripheral is phisically attached.
    sio_channel_try_write_t putc;
} SIOChannel;

#define SIO2_REG_NUM 4

#define SIO2_CHA_DATA_REG    (0x00)
#define SIO2_CHA_CONTROL_REG (0x01)
#define SIO2_CHB_DATA_REG    (0x02)
#define SIO2_CHB_CONTROL_REG (0x03)

// Read Register 0
#define RX_CHAR_AVAILABLE_BIT     (0)
#define PENDING_INTERRUPT_CHA_BIT (1)
#define TX_BUFFER_EMPTY_BIT       (2)
// Read Register 1
#define PARITY_ERROR_BIT  (4)
#define RX_OVERRUN_BIT    (5)
#define FRAMING_ERROR_BIT (6)
#define END_OF_FRAME_BIT  (7)
// Read Register 2
// Only available for Channel B, holds the interrupt vector octet.

#define CHANNEL_A (0)
#define CHANNEL_B (1)
static SIOChannel channels[2];

// vector byte to pass back to Z80 when an interrupt must be generated
static uint8_t sio_interrupt_vector = 0;
// true if waiting for acknoweledgment from Z80, false otherwise
static bool sio2_pending_interrupt = false;

/**
 * @brief Reinitialize an already initialized channel.
 *
 * This preserves important fields such as callback routines for the connected
 * device.
 *
 * @param channel Pointer to the channel to be reinitialized.
 */
static void sio_channel_reinit(SIOChannel *channel) {
    channel->reg_index = 0;
    memset(&channel->read_regs, 0, sizeof(channel->read_regs));
    FIFO_INIT(&channel->rx_fifo);
    FIFO_INIT(&channel->tx_fifo);
    channel->rx_enabled = false;
    channel->tx_enabled = false;
    channel->rx_int_enabled = false;
    channel->tx_int_enabled = false;
}

/**
 * @brief Initialize a channel struct.
 *
 * @param channel Pointer to channel struct.
 */
static void sio_channel_init(SIOChannel *channel) {
    memset(channel, 0, sizeof(*channel));
    sio_channel_reinit(channel);
}

static us_interval_t sio2_remaining(void) {
    // SIO2 state can not change faster than the minimum amount needed
    // to transmit (or receive) a frame via serial (10 bits), so there is
    // no point in polling faster
#define SIO2_MAX_BAUD_RATE (19200)
#define SERIAL_FRAME_MIN_DURATION_US                                           \
    (1000ULL * 1000ULL / SIO2_MAX_BAUD_RATE * 10)

    return SERIAL_FRAME_MIN_DURATION_US;
}

static uint8_t sio_channel_read_data(SIOChannel *channel) {
    if (!FIFO_ISEMPTY(&channel->rx_fifo)) {
        const uint8_t c = FIFO_POP(&channel->rx_fifo);

        // reset "data available" bit in read reg 0
        if (FIFO_ISEMPTY(&channel->rx_fifo))
            channel->read_regs[0] &= (uint8_t) ~(1 << RX_CHAR_AVAILABLE_BIT);

        return c;
    }

    // TODO(giomba): what happens when you try to read from a channel and there
    // is no data available? Maybe you read the last received byte twice? Maybe
    // it depends on the SIO silicon revision?
    return 0x55;
}

static void sio_channel_write_data(SIOChannel *channel, uint8_t value) {
    if (FIFO_ISFULL(&channel->tx_fifo))
        return;

    FIFO_PUSH(&channel->tx_fifo, value);

    if (FIFO_ISFULL(&channel->tx_fifo)) {
        // TODO(giomba): change shift register free status bit
    }
}

static uint8_t sio_channel_read_control(SIOChannel *channel) {
#if 0
    LOG_DEBUG("read control: channel = %c, reg_index = %d\n",
              (channel == &channels[CHANNEL_A]) ? 'A' : 'B',
              channel->reg_index);
#endif

    // avoid reading garbage if a write-only register is indexed
    if (channel->reg_index >= ARRAY_SIZE(channel->read_regs))
        return 0x55;

    return channel->read_regs[channel->reg_index];
}

static void write_register_0(SIOChannel *channel, uint8_t value) {
#if 0
        // Execute additional command.
        // Additional commands must be executed before normal commands,
        // because normal commands can execute a reset,
        // which will void anything done here.
        switch (value >> 6 & 0x3) {
        case 1:
            // reset RX CRC check
            break;
        case 2:
            // reset TX CRC generator
            break;
        case 3:
            // reset TX underrun latch
            break;
        }
#endif

    // Execute normal command.
    switch (value >> 3 & 0x7) {
    case 2:
        // reset interrupts status
        sio2_pending_interrupt = false;
        // Note: maybe we should also stop pulling the IRQ line low,
        // but... we need an interface to pull interrupt requests back
        // from the bus, first.
        break;
    case 3:
        // reset channel
        sio_channel_reinit(channel);
        LOG_DEBUG("sio channel reset, reg_index = %d\n", channel->reg_index);
        break;
    case 4:
        // enable RX interrupt
        (void)4;
        break;
    case 5:
        // reset pending TX interrupt
        (void)5;
        break;
    case 6:
        // reset error
        (void)6;
        break;
    case 7:
        // return from INT (only channel A)
        (void)7;
        break;

    default:
        break;
    }
}

static void write_register_1(SIOChannel *channel, uint8_t value) {
    switch (value >> 3 & 0x3) {
    case 0:
        // RX interrupt disable
        LOG_DEBUG("sio2: disable interrupts channel %c\n",
                  (channel == &channels[CHANNEL_A]) ? 'A' : 'B');
        channel->rx_int_enabled = false;
        break;
    case 1:
        // RX interrupt on first character received
        // TODO(giomba): to be implemented
        break;
#if 0
        case 2:
            // RX interrupt on all received characters (parity affects vector)
            // Do we need to implement this? I don't think so, since parity
            // errors cannot be actually produced.
            break;
#endif
    case 3:
        // RX interrupt on all received characters.
        // (parity does not affect vector)
        LOG_DEBUG("sio2: enable interrupts channel %c\n",
                  (channel == &channels[CHANNEL_A]) ? 'A' : 'B');
        channel->rx_int_enabled = true;
        break;
    }
}

static void write_register_2(SIOChannel *channel, uint8_t value) {
    (void)channel;
    // SIO/2 interrupt vector
    sio_interrupt_vector = value;
}

static void write_register_3(SIOChannel *channel, uint8_t value) {
    // RX enable
    channel->rx_enabled = value & 0x1;

    // only 8 bit bytes are supported in this emulator
    if ((value >> 6 & 0x3) != 3)
        LOG_WARN("SIO/2 configured to receive with byte width != 8 bit\n");
}

static void write_register_4(SIOChannel *channel, uint8_t value) {
    // not implemented
    (void)channel;
    (void)value;
}

static void write_register_5(SIOChannel *channel, uint8_t value) {
    // TX enable
    channel->tx_enabled = value & 0x8;

    if ((value >> 5 & 0x3) != 3)
        LOG_WARN("SIO/2 configured to transmit with byte width != 8 bit\n");
    channel->reg_index = 0;
}

static void write_register_6(SIOChannel *channel, uint8_t value) {
    // not implemented
    (void)channel;
    (void)value;
}

static void write_register_7(SIOChannel *channel, uint8_t value) {
    // not implemented
    (void)channel;
    (void)value;
}

typedef void (*write_register_handler_t)(SIOChannel *, uint8_t);

static const write_register_handler_t write_register_handlers[] = {
    write_register_0, write_register_1, write_register_2, write_register_3,
    write_register_4, write_register_5, write_register_6, write_register_7,
};

static void sio_channel_write_control(SIOChannel *channel, uint8_t value) {
    uint8_t indexed = 0;

    // select register when writing to write register 0
    if (channel->reg_index == 0)
        indexed = value & 0x7;

    const write_register_handler_t handler =
        write_register_handlers[channel->reg_index];
    handler(channel, value);

    channel->reg_index = indexed;
}

uint8_t sio2_in(ceda_ioaddr_t address) {
    assert(address < SIO2_REG_NUM);

#if 0
    LOG_DEBUG("sio2 in: address = %02x\n", address);
#endif

    if (address == SIO2_CHA_DATA_REG) {
        // TODO(giomba): read external RS232
        return sio_channel_read_data(&channels[CHANNEL_A]);
    }
    if (address == SIO2_CHA_CONTROL_REG) {
        return sio_channel_read_control(&channels[CHANNEL_A]);
    }
    if (address == SIO2_CHB_DATA_REG) {
        // TODO(giomba): read keyboard input
        return sio_channel_read_data(&channels[CHANNEL_B]);
    }
    if (address == SIO2_CHB_CONTROL_REG) {
        return sio_channel_read_control(&channels[CHANNEL_B]);
    }

    assert(0);
    return 0x00;
}

void sio2_out(ceda_ioaddr_t address, uint8_t value) {
    assert(address < SIO2_REG_NUM);

    LOG_DEBUG("sio2 out: address = %02x, value = %02x\n", address, value);

    if (address == SIO2_CHA_DATA_REG) {
        // TODO(giomba): write external RS232
        sio_channel_write_data(&channels[CHANNEL_A], value);
    } else if (address == SIO2_CHA_CONTROL_REG) {
        sio_channel_write_control(&channels[CHANNEL_A], value);
    } else if (address == SIO2_CHB_DATA_REG) {
        // TODO(giomba): write to keyboard/auxiliary serial
        sio_channel_write_data(&channels[CHANNEL_B], value);
    } else if (address == SIO2_CHB_CONTROL_REG) {
        sio_channel_write_control(&channels[CHANNEL_B], value);
    } else {
        assert(0);
    }

    (void)value;
}

static void sio2_start(void) {
    // acquire dynamic resources
    // (nothing to do at the moment)
}

static void sio2_cleanup(void) {
    // release dynamic resources
    // (nothing to do at the moment)
}

static void sio2_irq_ack(void) {
    sio2_pending_interrupt = false;
}

static void sio2_poll(void) {
    // try to read data from external serial peripherals
    for (size_t i = 0; i < ARRAY_SIZE(channels); ++i) {
        SIOChannel *channel = &channels[i];
        // no peripheral phisically attached
        if (!channel->getc)
            continue;

        // Not enough space in RX FIFO, skip.
        // This does not actually happen on real hardware, but
        // do we really want to handle the buffer overrun condition?
        if (FIFO_ISFULL(&channel->rx_fifo))
            continue;

        // try get char from peripheral
        uint8_t c;
        const bool ok = channel->getc(&c);
        if (!ok) // no char available
            continue;

        // if receiver is disabled, discard incoming data
        if (!channel->rx_enabled)
            continue;

        LOG_DEBUG("sio2: channel %zu: received char: %02x\n", i, c);

        // put char in RX fifo
        FIFO_PUSH(&channel->rx_fifo, c);

        // signal RX char available
        channel->read_regs[0] |= (1U << RX_CHAR_AVAILABLE_BIT);

        LOG_DEBUG("interrupt enabled: %d\n", channel->rx_int_enabled);

        // generate interrupt request, if interrupts are enabled
        if (channel->rx_int_enabled) {
            if (!sio2_pending_interrupt) {
                LOG_DEBUG("sio2: send interrupt!\n");
                bus_intPush(sio_interrupt_vector, sio2_irq_ack);
            }
            sio2_pending_interrupt = true;
        }
    }

    // try to write data to external serial peripherals
    for (size_t i = 0; i < ARRAY_SIZE(channels); ++i) {
        SIOChannel *channel = &channels[i];
        // no peripheral phisically attached
        if (!channel->putc)
            continue;

        // skip if transmitter is not enabled
        if (!channel->tx_enabled)
            continue;

        // nothing to send to this peripheral
        if (FIFO_ISEMPTY(&channel->tx_fifo))
            continue;

        // try put char to peripheral
        const uint8_t c = FIFO_PEEK(&channel->tx_fifo);
        const bool ok = channel->putc(c);
        if (!ok)
            continue;

        // actually remove char from TX FIFO
        (void)FIFO_POP(&channel->tx_fifo);
    }
}

void sio2_init(CEDAModule *mod) {
    mod->init = sio2_init;
    mod->start = sio2_start;
    mod->poll = sio2_poll;
    mod->remaining = sio2_remaining;
    mod->cleanup = sio2_cleanup;

    for (size_t i = 0; i < ARRAY_SIZE(channels); ++i)
        sio_channel_init(&channels[i]);

    // attach keyboard to channel B
    channels[CHANNEL_B].getc = keyboard_getChar;
}
