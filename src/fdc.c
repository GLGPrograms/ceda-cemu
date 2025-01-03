#include "fdc.h"

#include <assert.h>
#include <string.h>

#include "floppy.h"
#include "macro.h"

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

uint8_t tc_status = 0;

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

// Some commands carry argument bits in MSb
#define CMD_COMMAND_bm (0x1F)
#define CMD_ARGS_bm    (0xE0)
#define CMD_ARGS_MT_bm (0x80)
#define CMD_ARGS_MF_bm (0x40)
#define CMD_ARGS_SK_bm (0x20)

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

// Parsing structure for read and write arguments
// TODO(giuliof): this is not portable
typedef struct rw_args_t {
    uint8_t unit_select : 2;
    uint8_t head_address : 1;
    uint8_t : 0;
    uint8_t cylinder;
    uint8_t head;
    uint8_t record;
    uint8_t n;
    uint8_t eot;
    uint8_t gpl;
    uint8_t stp;
} rw_args_t;

/* Command callbacks prototypes */
static void pre_exec_specify(void);
static void pre_exec_write_data(void);
static uint8_t exec_write_data(uint8_t value);
static void post_exec_write_data(void);
static void pre_exec_read_data(void);
static uint8_t exec_read_data(uint8_t value);
static void post_exec_read_data(void);
static void pre_exec_recalibrate(void);
static void post_exec_sense_interrupt(void);
static void pre_exec_format_track(void);
static uint8_t exec_format_track(uint8_t value);
static void post_exec_format_track(void);
static void pre_exec_seek(void);
/* Utility routines prototypes */
static void fdc_compute_next_status(void);
// Update read buffer with the data from current ths
static void buffer_update(void);
static void buffer_write_size(void);

/* Local variables */
// The command descriptors
static const fdc_operation_t fdc_operations[] = {
    {
        .cmd = SPECIFY,
        .args_len = 2,
        .result_len = 0,
        .pre_exec = pre_exec_specify,
        .exec = NULL,
        .post_exec = NULL,
    },
    {
        .cmd = WRITE_DATA,
        .args_len = 8,
        .result_len = 7,
        .pre_exec = pre_exec_write_data,
        .exec = exec_write_data,
        .post_exec = post_exec_write_data,
    },
    {
        .cmd = READ_DATA,
        .args_len = 8,
        .result_len = 7,
        .pre_exec = pre_exec_read_data,
        .exec = exec_read_data,
        .post_exec = post_exec_read_data,
    },
    {
        .cmd = RECALIBRATE,
        .args_len = 1,
        .result_len = 0,
        .pre_exec = pre_exec_recalibrate,
        .exec = NULL,
        .post_exec = NULL,
    },
    {
        .cmd = SENSE_INTERRUPT,
        .args_len = 0,
        .result_len = 2,
        .pre_exec = NULL,
        .exec = NULL,
        .post_exec = post_exec_sense_interrupt,
    },
    {
        .cmd = FORMAT_TRACK,
        .args_len = 5,
        .result_len = 7,
        .pre_exec = pre_exec_format_track,
        .exec = exec_format_track,
        .post_exec = post_exec_format_track,
    },
    {
        .cmd = SEEK,
        .args_len = 2,
        .result_len = 0,
        .pre_exec = pre_exec_seek,
        .exec = NULL,
        .post_exec = NULL,
    },
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
// Execution buffer, will keep sector's information
// TODO(giuliof): at the moment its size is the maximum allowed on CEDA, but
// FDC can theoretically handle bigger sector sizes
static uint8_t exec_buffer[1024];
// Result buffer. Each command has maximum 7 bytes as argument.
static uint8_t result[7];
static bool isReady = false;

/* FDC internal registers */
// Main Status Register
static main_status_register_t status_register;

/* Floppy disk status */
// Current track position
static uint8_t track[4];

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

// Write data:
static void pre_exec_write_data(void) {
    // TODO(giuliof): eventually: use the appropriate structure
    LOG_DEBUG("FDC Write Data\n");
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

    // Set DIO to read for Execution phase
    status_register.dio = 1;

    buffer_write_size();
}

static uint8_t exec_write_data(uint8_t value) {
    rw_args_t *rw_args = (rw_args_t *)args;

    exec_buffer[rwcount++] = value;

    if (rwcount == rwcount_max) {
        uint8_t sector = rw_args->record;
        // FDC counts sectors from 1
        assert(sector != 0);
        // But all other routines counts sectors from 0
        sector--;
        int ret = floppy_write_buffer(exec_buffer, rw_args->unit_select,
                                      rw_args->head_address,
                                      track[rw_args->unit_select],
                                      rw_args->head, rw_args->cylinder, sector);
        // TODO(giuliof): At the moment we do not support error codes, we assume
        // the image is always loaded and valid
        CEDA_STRONG_ASSERT_TRUE(ret > 0);
        // Buffer is statically allocated, be sure that the data can fit it
        CEDA_STRONG_ASSERT_TRUE((size_t)ret <= sizeof(exec_buffer));

        // Multi-sector mode (enabled by default).
        // If read is not interrupted at the end of the sector, the next logical
        // sector is loaded
        rw_args->record++;

        // Last sector of the track
        if (rw_args->record > rw_args->eot) {
            // Multi track mode, if enabled the read operation go on on the next
            // side of the same track
            if (command_args & CMD_ARGS_MT_bm) {
                rw_args->head_address = !rw_args->head_address;
                rw_args->head = !rw_args->head;
            };

            // In any case, reached the end of track we start back from sector 1
            rw_args->record = 1;
        }

        buffer_write_size();
    }
    return 0;
}

static void post_exec_write_data(void) {
    LOG_DEBUG("Write has ended\n");
    // TODO(giulio): populate result (which is pretty the same for read,
    // write, ...)
    memset(result, 0x00, sizeof(result));
}

// Read data:
static void pre_exec_read_data(void) {
    // TODO(giuliof): eventually: use the appropriate structure
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

    // Set DIO to read for Execution phase
    status_register.dio = 1;

    // TODO(giuliof) create handles to manage more than one floppy image at a
    // time
    // floppy_read_buffer(exec_buffer, track_size, head, track, sector);
    // TODO(giuliof): may be a good idea to pass a sort of "floppy context"
    buffer_update();
}

static uint8_t exec_read_data(uint8_t value) {
    rw_args_t *rw_args = (rw_args_t *)args;

    // read doesn't care of in value
    (void)value;
    uint8_t ret = exec_buffer[rwcount++];

    if (rwcount == rwcount_max) {
        // Multi-sector mode (enabled by default).
        // If read is not interrupted at the end of the sector, the next logical
        // sector is loaded
        rw_args->record++;

        // Last sector of the track
        if (rw_args->record > rw_args->eot) {
            // Multi track mode, if enabled the read operation go on on the next
            // side of the same track
            if (command_args & CMD_ARGS_MT_bm) {
                rw_args->head_address = !rw_args->head_address;
                rw_args->head = !rw_args->head;
            };

            // In any case, reached the end of track we start back from sector 1
            rw_args->record = 1;
        }

        buffer_update();
    }
    return ret;
}

static void post_exec_read_data(void) {
    LOG_DEBUG("Read has ended\n");
    // TODO(giulio): populate result (which is pretty the same for read,
    // write, ...)
    memset(result, 0x00, sizeof(result));

    // TODO(giuliof): populate result as in datasheet (see table 2)
}

// Recalibrate:
// Just print the register values.
static void pre_exec_recalibrate(void) {
    uint8_t drive = args[0] & 0x3;

    LOG_DEBUG("FDC Recalibrate\n");
    LOG_DEBUG("Drive: %d\n", drive);

    track[drive] = 0;

    // We don't have to actually move the head. The drive is immediately ready
    isReady = true;
}

// Sense interrupt:
static void post_exec_sense_interrupt(void) {
    // TODO(giuliof): last accessed drive
    uint8_t drive = 0;

    // After reading interrupt status, ready can be deasserted
    isReady = false;

    LOG_DEBUG("FDC Sense Interrupt\n");
    /* Status Register 0 */
    // Drive number
    result[0] = drive;
    // head address (last addressed) - TODO(giulio)
    // result[0] |= ...;
    // Seek End - TODO(giulio)
    result[0] |= 1U << 5;
    /* PCN  - (current track position) */
    result[1] = track[drive];
}

// Format track
static void pre_exec_format_track(void) {
    LOG_DEBUG("FDC Format track\n");
}

static uint8_t exec_format_track(uint8_t value) {
    return value;
}

static void post_exec_format_track(void) {
    LOG_DEBUG("FDC end Format track\n");
}

// Seek
static void pre_exec_seek(void) {
    uint8_t drive = args[0] & 0x03;
    track[drive] = args[1];

    LOG_DEBUG("FDC Seek\n");
    LOG_DEBUG("Drive: %d\n", drive);
    LOG_DEBUG("HD: %d\n", (result[0] >> 2) & 0x01);
    LOG_DEBUG("NCN: %d\n", track[drive]);

    // We don't have to actually move the head. The drive is immediately ready
    isReady = true;
}

/* * * * * * * * * * * * * * *  Utility routines  * * * * * * * * * * * * * * */

static void fdc_compute_next_status(void) {
    if (!fdc_currop)
        return;

    // rwcount during execution phase is handled directly by the exec callback
    if (fdc_status != EXEC)
        rwcount++;

    if (fdc_status == CMD) {
        // Set DIO to write for ARGS phase
        status_register.dio = 0;

        fdc_status = ARGS;
        rwcount_max = fdc_currop->args_len;
        rwcount = 0;
    }

    if (fdc_status == ARGS && rwcount == rwcount_max) {
        fdc_status = EXEC;

        // exec should set DIO according to direction
        if (fdc_currop->pre_exec)
            fdc_currop->pre_exec();

        rwcount = 0;
    }

    if (fdc_status == EXEC && (tc_status || fdc_currop->exec == NULL)) {
        tc_status = 0;
        // Set DIO to read for RESULT phase
        status_register.dio = 1;

        if (fdc_currop->post_exec)
            fdc_currop->post_exec();

        fdc_status = RESULT;
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

static void buffer_update(void) {
    rw_args_t *rw_args = (rw_args_t *)args;

    uint8_t sector = rw_args->record;

    // FDC counts sectors from 1
    assert(sector != 0);

    // But all other routines counts sectors from 0
    sector--;

    ssize_t ret = floppy_read_buffer(
        NULL, rw_args->unit_select, rw_args->head_address,
        track[rw_args->unit_select], rw_args->head, rw_args->cylinder, sector);

    if (ret != 0) {
        // TODO(giuliof): At the moment we do not support error codes, we assume
        // the image is always loaded and valid
        CEDA_STRONG_ASSERT_TRUE(ret > 0);
        // Buffer is statically allocated, be sure that the data can fit it
        CEDA_STRONG_ASSERT_TRUE((size_t)ret <= sizeof(exec_buffer));

        ret = floppy_read_buffer(exec_buffer, rw_args->unit_select,
                                 rw_args->head_address,
                                 track[rw_args->unit_select], rw_args->head,
                                 rw_args->cylinder, sector);
        // TODO(giuliof): At the moment we do not support error codes, we assume
        // the image is always loaded and valid
        CEDA_STRONG_ASSERT_TRUE(ret >= 0);
        // Ready to serve data
        isReady = true;
    }
    // TODO(giuliof): add proper error code, this is no mounted image
    else {
        // Not ready to serve data
        isReady = false;
    }

    rwcount = 0;
    // TODO(giuliof) rwcount_max = min(DTL, ret)
    rwcount_max = (size_t)ret;
}

static void buffer_write_size(void) {
    rw_args_t *rw_args = (rw_args_t *)args;

    uint8_t sector = rw_args->record;

    // FDC counts sectors from 1
    assert(sector != 0);

    // But all other routines counts sectors from 0
    sector--;

    int ret = floppy_write_buffer(
        NULL, rw_args->unit_select, rw_args->head_address,
        track[rw_args->unit_select], rw_args->head, rw_args->cylinder, sector);

    // TODO(giuliof): At the moment we do not support error codes, we assume the
    // image is always loaded and valid
    CEDA_STRONG_ASSERT_TRUE(ret > 0);
    // Buffer is statically allocated, be sure that the data can fit it
    CEDA_STRONG_ASSERT_TRUE((size_t)ret <= sizeof(exec_buffer));

    rwcount = 0;
    // TODO(giuliof) rwcount_max = min(DTL, ret)
    rwcount_max = (size_t)ret;
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
            uint8_t cmd = value & CMD_COMMAND_bm;
            command_args = value & CMD_ARGS_bm;

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

// This IO line is directly connected to TC (Terminal Count) pin, which stops
// the exec step.
void fdc_tc_out(ceda_ioaddr_t address, uint8_t value) {
    (void)address;
    (void)value;

    if (fdc_status == EXEC) {
        // TODO(giuliof) tc may be an argument to the fdc_compute_next_status,
        // since it is just a trigger.
        tc_status = 1;
        fdc_compute_next_status();
    }
}

// TODO(giuliof): After Execution Phase or EOR sector read, INT=1
// (beginning of result phase). When first byte of result phase data
// is read, INT=0.
bool fdc_getIntStatus(void) {
    return isReady;
}

void fdc_kickDiskImage(void) {
    if (fdc_status == EXEC && fdc_currop->cmd == READ_DATA) {
        buffer_update();
    }

    if (fdc_status == EXEC && fdc_currop->cmd == WRITE_DATA) {
        buffer_write_size();
    }
}
