#include "fdc.h"

#include <assert.h>
#include <string.h>

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

// The FDC virtually expose two registers, which can be both read or written
#define ADDR_STATUS_REGISTER (0x00)
#define ADDR_DATA_REGISTER   (0x01)

// The FDC can perform 15 different commands.
// Don't ask me why they didn't use a single nibble to represent all the
// commands. Please refer to the datasheet for more information.
typedef enum cmd_t {
    READ_TRACK = 0x02,
    SPECIFY = 0x03,
    SENSE_DRIVE = 0x04,
    WRITE_DATA = 0x05,
    READ_DATA = 0x06,
    RECALIBRATE = 0x07,
    SENSE_INTERRUPT = 0x08,
    WRITE_DELETED_DATA = 0x09,
    READ_ID = 0x0A,
    READ_DELETED_DATA = 0x0C,
    FORMAT_TRACK = 0x0D,
    SEEK = 0x0F,
    SCAN_EQUAL = 0x11,
    SCAN_LOW_EQUAL = 0x19,
    SCAN_HIGH_EQUAL = 0x1D
} cmd_t;

// Each FDC command sequence can be split in four phases.
// The single-byte command must be always sent (CMD).
// Optional arguments may follow (ARGS).
// While performing the requested operation, the FDC may request additional
// data transfer (EXEC).
// After completion of all operations, status and other information are made
// available to the processor (RESULT).
typedef enum fdc_status_t { CMD, ARGS, EXEC, RESULT } fdc_status_t;

// Operation descriptor.
// Each operation has an associated command and a variable length argument,
// execution and resul phases.
// One may provide callbacks for exec preparation, actual execution and
// post execution.
typedef struct fdc_operation_t {
    cmd_t cmd;
    // An FDC operation is splitted into 4 steps.
    // If a step len is 0, that step must be skipped.
    size_t args_len;
    size_t exec_len;
    size_t result_len;
    // Called when args fetching is ended
    void (*pre_exec)(void);
    // Called during execution phase
    uint8_t (*exec)(uint8_t);
    // Called when execution phase is ended (even if no execution is present,
    // just to prepare result values)
    void (*post_exec)(void);
} fdc_operation_t;

// Main status register bitfield
typedef union main_status_register_t {
    uint8_t value;
    struct {
        // Drive x is in Seek mode
        uint8_t fdd0_busy : 1;
        uint8_t fdd1_busy : 1;
        uint8_t fdd2_busy : 1;
        uint8_t fdd3_busy : 1;
        // Controller has already accepted a command
        uint8_t fdc_busy : 1;
        // Execution mode
        uint8_t exm : 1;
        // Data I/O, set if FDC is read from CPU, clear otherwise
        uint8_t dio : 1;
        // Request From Master, set if FDC is ready to receive or send data
        uint8_t rqm : 1;
        uint8_t : 0;
    };
} main_status_register_t;

/* Command callbacks prototypes */
static void pre_exec_specify(void);
static void pre_exec_read_data(void);
static uint8_t exec_read_data(uint8_t value);
static void post_exec_read_data(void);
static void pre_exec_recalibrate(void);
static void post_exec_sense_interrupt(void);

/* Local variables */
// The command descriptors
static const fdc_operation_t fdc_operations[] = {
    {.cmd = SPECIFY,
     .args_len = 2,
     .exec_len = 0,
     .result_len = 0,
     .pre_exec = pre_exec_specify,
     .exec = NULL,
     .post_exec = NULL},
    {.cmd = READ_DATA,
     .args_len = 8,
     .exec_len = 0,
     .result_len = 7,
     .pre_exec = pre_exec_read_data,
     .exec = exec_read_data,
     .post_exec = post_exec_read_data},
    {.cmd = RECALIBRATE,
     .args_len = 1,
     .exec_len = 0,
     .result_len = 0,
     .pre_exec = pre_exec_recalibrate,
     .exec = NULL,
     .post_exec = NULL},
    {.cmd = SENSE_INTERRUPT,
     .args_len = 0,
     .exec_len = 0,
     .result_len = 2,
     .pre_exec = NULL,
     .exec = NULL,
     .post_exec = post_exec_sense_interrupt},
};
// Current FDC status
static fdc_status_t fdc_status = CMD;
// Currently selected operation
static const fdc_operation_t *fdc_currop;
// Some commands have arguments inside command byte too
static uint8_t command_args = 0;
// Keeps the count of the read/write accesses among the current status
static size_t rwcount = 0;
// The top value of read or write operation among the current status
static size_t rwcount_max = 0;
// Arguments buffer. Each command has maximum 8 bytes as argument.
static uint8_t args[8];
// Result buffer. Each command has maximum 7 bytes as argument.
static uint8_t result[7];

/* FDC internal registers */
// Main Status Register
static main_status_register_t status_register;

/* * * * * * * * * * * * * * *  Command routines  * * * * * * * * * * * * * * */

// Specify:
// Just print the register values, since the emulator does not care
static void pre_exec_specify(void) {
    LOG_DEBUG("FDC Specify\n");
    LOG_DEBUG("HUT: %d\n", args[0] & 0xF);
    LOG_DEBUG("SRT: %d\n", args[0] >> 4);
    LOG_DEBUG("ND: %d\n", args[1] & 1);
    LOG_DEBUG("HLT: %d\n", args[1] >> 1);
}

// Read data:
static void pre_exec_read_data(void) {
    LOG_DEBUG("FDC Read Data\n");
    LOG_DEBUG("SK: %d\n", (command_args >> 5) & 0x01);
    LOG_DEBUG("MF: %d\n", (command_args >> 6) & 0x01);
    LOG_DEBUG("MT: %d\n", (command_args >> 7) & 0x01);
    LOG_DEBUG("Drive: %d\n", args[0] & 0x3);
    LOG_DEBUG("HD: %d\n", (args[0] >> 2) & 0x1);
    LOG_DEBUG("Cyl: %d\n", args[1]);
    LOG_DEBUG("Head: %d\n", args[2]);
    LOG_DEBUG("Record: %d\n", args[3]);
    LOG_DEBUG("N: %d\n", args[4]);
    LOG_DEBUG("EOT: %d\n", args[5]);
    LOG_DEBUG("GPL: %d\n", args[6]);
    LOG_DEBUG("DTL: %d\n", args[7]);

    // let's use DTL as read data
    rwcount_max = args[7] + 1;
    // Set DIO to read for Execution phase
    status_register.dio = 1;
}

static uint8_t exec_read_data(uint8_t value) {
    // read doesn't care of in value
    (void)value;

    // TODO(giulio): just serve HALT opcode
    return 0x76;
}

static void post_exec_read_data(void) {
    LOG_DEBUG("Read has ended\n");
    // TODO(giulio): populate result (which is pretty the same for read,
    // write, ...)
    memset(result, 0x00, sizeof(result));
}

// Recalibrate:
// Just print the register values.
// TODO(giuliof): actually set drive x's track to 0
static void pre_exec_recalibrate(void) {
    LOG_DEBUG("FDC Recalibrate\n");
    LOG_DEBUG("Drive: %d\n", args[0] & 0x3);
}

// Sense interrupt:
static void post_exec_sense_interrupt(void) {
    LOG_DEBUG("FDC Sense Interrupt\n");
    /* Status Register 0 */
    // Drive number, head address (last addressed) - TODO(giulio)
    result[0] = 0x00;
    // Seek End - TODO(giulio)
    result[0] |= 1U << 5;
    /* PCN  - (current track position) */
    // TODO(giulio)
    result[1] = 0x00;
}

/* * * * * * * * * * * * * * *  Utility routines  * * * * * * * * * * * * * * */

static void fdc_compute_next_status(void) {
    rwcount++;

    if (fdc_status == CMD) {
        // Set DIO to write for ARGS phase
        status_register.dio = 0;

        fdc_status = ARGS;
        if (fdc_currop)
            rwcount_max = fdc_currop->args_len;
        rwcount = 0;
    }

    if (fdc_status == ARGS && rwcount == rwcount_max) {
        fdc_status = EXEC;
        if (fdc_currop)
            rwcount_max = fdc_currop->exec_len;

        // exec should set DIO according to direction
        if (fdc_currop && fdc_currop->pre_exec)
            fdc_currop->pre_exec();

        rwcount = 0;
    }

    if (fdc_status == EXEC && rwcount == rwcount_max) {
        // Set DIO to read for RESULT phase
        status_register.dio = 1;

        if (fdc_currop && fdc_currop->post_exec)
            fdc_currop->post_exec();

        fdc_status = RESULT;
        if (fdc_currop)
            rwcount_max = fdc_currop->result_len;
        rwcount = 0;
    }

    if (fdc_status == RESULT && rwcount == rwcount_max) {
        // Set DIO to write for CMD and ARGS phases
        status_register.dio = 0;

        fdc_status = CMD;
        rwcount_max = 0;
        rwcount = 0;
    }

    // Update step dependant bits in main status register
    status_register.exm = fdc_status == EXEC;
    status_register.fdc_busy = fdc_status != CMD;
}

/* * * * * * * * * * * * * * *  Public routines   * * * * * * * * * * * * * * */

void fdc_init(void) {
    fdc_status = CMD;
    rwcount_max = 0;
    status_register.value = 0x00;
    // FDC is always ready
    status_register.rqm = 1;
}

uint8_t fdc_in(ceda_ioaddr_t address) {
    switch (address & 0x01) {
    case ADDR_STATUS_REGISTER:
        return status_register.value;
    case ADDR_DATA_REGISTER: {
        uint8_t value = 0;

        if (fdc_status == CMD) {
            // you should never read during command phase
            LOG_WARN("FDC read access during CMD phase!\n");
        } else if (fdc_status == ARGS) {
            // you should never read during command phase
            LOG_WARN("FDC read access during ARGS phase!\n");
        } else if (fdc_status == EXEC) {
            // TODO(giuliof) Check if direction is correct (if sr.DIO == 0,
            // return)

            if (fdc_currop && fdc_currop->exec)
                value = fdc_currop->exec(0);
            else
                LOG_ERR("Exec unassigned for FDC when writing");
        } else if (fdc_status == RESULT) {
            assert(rwcount < sizeof(result) / sizeof(*result));
            value = result[rwcount];
        }

        fdc_compute_next_status();

        return value;
    } break;
    }

    return 0x00;
}

void fdc_out(ceda_ioaddr_t address, uint8_t value) {
    switch (address & 0x01) {
    case ADDR_STATUS_REGISTER:
        LOG_WARN("nobody should write in FDC main status register\n");
        return;
    case ADDR_DATA_REGISTER: {
        if (fdc_status == CMD) {
            // Split the command itself from option bits
            uint8_t cmd = value & 0x1F;
            command_args = value & 0xE0;

            // Unroll the command list and place it in the current execution
            fdc_currop = NULL;
            for (size_t i = 0;
                 i < sizeof(fdc_operations) / sizeof(*fdc_operations); i++) {
                if (cmd == fdc_operations[i].cmd) {
                    fdc_currop = &fdc_operations[i];
                    break;
                }
            }
            if (fdc_currop == NULL)
                LOG_WARN("Command %x is not implemented\n", cmd);
        } else if (fdc_status == ARGS) {
            assert(rwcount < sizeof(args) / sizeof(*args));
            args[rwcount] = value;
        } else if (fdc_status == EXEC) {
            // TODO(giuliof) Check if direction is correct (if sr.DIO == 1,
            // return)

            if (fdc_currop && fdc_currop->exec)
                fdc_currop->exec(value);
            else
                LOG_ERR("Exec unassigned for FDC when writing");
        } else if (fdc_status == RESULT) {
            // you should never write during result phase
            LOG_WARN("FDC write access during RESULT phase!\n");
        }

        fdc_compute_next_status();
    } break;
    }
}
