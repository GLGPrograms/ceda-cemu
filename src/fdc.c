#include "fdc.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "fdc_registers.h"
#include "macro.h"

#define LOG_LEVEL LOG_LVL_DEBUG
#include "log.h"

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
    fdc_cmd_t cmd;
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

// Parsing structure for read and write arguments
typedef struct rw_args_t {
    uint8_t unit_head;
    uint8_t cylinder;
    uint8_t head;
    uint8_t record;
    uint8_t n;
    uint8_t eot;
    uint8_t gpl;
    uint8_t stp;
} rw_args_t;

// Parsing structure for format arguments
typedef struct format_args_t {
    uint8_t unit_head;
    uint8_t n;             // bytes per sector factor
    uint8_t sec_per_track; // number of sectors per track
    uint8_t gpl;           // gap length
    uint8_t d;             // filler byte
} format_args_t;

/* Command callbacks prototypes */
static void pre_exec_read_track(void);
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
        .cmd = FDC_READ_TRACK,
        .args_len = 8,
        .result_len = 7,
        .pre_exec = pre_exec_read_track,
        .exec = exec_read_data,           // same as read data
        .post_exec = post_exec_read_data, // same as read data
    },
    {
        .cmd = FDC_SPECIFY,
        .args_len = 2,
        .result_len = 0,
        .pre_exec = pre_exec_specify,
        .exec = NULL,
        .post_exec = NULL,
    },
    {
        .cmd = FDC_WRITE_DATA,
        .args_len = 8,
        .result_len = 7,
        .pre_exec = pre_exec_write_data,
        .exec = exec_write_data,
        .post_exec = post_exec_write_data,
    },
    {
        .cmd = FDC_READ_DATA,
        .args_len = 8,
        .result_len = 7,
        .pre_exec = pre_exec_read_data,
        .exec = exec_read_data,
        .post_exec = post_exec_read_data,
    },
    {
        .cmd = FDC_RECALIBRATE,
        .args_len = 1,
        .result_len = 0,
        .pre_exec = pre_exec_recalibrate,
        .exec = NULL,
        .post_exec = NULL,
    },
    {
        .cmd = FDC_SENSE_INTERRUPT,
        .args_len = 0,
        .result_len = 2,
        .pre_exec = NULL,
        .exec = NULL,
        .post_exec = post_exec_sense_interrupt,
    },
    {
        .cmd = FDC_WRITE_DELETED_DATA,
        .args_len = 8,
        .result_len = 7,
        .pre_exec = pre_exec_write_data,
        .exec = exec_write_data,
        .post_exec = post_exec_write_data,
    },
    {
        .cmd = FDC_READ_DELETED_DATA,
        .args_len = 8,
        .result_len = 7,
        .pre_exec = pre_exec_read_data,
        .exec = exec_read_data,
        .post_exec = post_exec_read_data,
    },
    {
        .cmd = FDC_FORMAT_TRACK,
        .args_len = 5,
        .result_len = 7,
        .pre_exec = pre_exec_format_track,
        .exec = exec_format_track,
        .post_exec = post_exec_format_track,
    },
    {
        .cmd = FDC_SEEK,
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
static bool tc_status = false;
static bool int_status = false;

/* FDC internal registers */
// Main Status Register
static uint8_t status_register;

/* Floppy disk status */
// Current track position
static uint8_t track[4];

/* Callbacks to handle floppy read and write */
static fdc_read_write_t read_buffer_cb = NULL;
static fdc_read_write_t write_buffer_cb = NULL;

/* * * * * * * * * * * * * * *  Command routines  * * * * * * * * * * * * * * */

static void pre_exec_read_track(void) {
    // TODO(giuliof): if I have understood correctly, this is just a read
    // command that ignores the record. I can just force it to 1 (first) and go
    // on as in read data.
    args[3] = 1;
    pre_exec_read_data();

    // TODO(giuliof): another small differences, to be checked, are that this
    // command doesn't stop if an error occurs, but stops once reached record =
    // EOT.
}

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
    status_register &= (uint8_t)~FDC_ST_DIO;

    buffer_write_size();
}

static uint8_t exec_write_data(uint8_t value) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;

    if (write_buffer_cb == NULL)
        return 0;

    if (rwcount_max == 0) {
        LOG_WARN("Write execution happened when no data can be written");
        return 0;
    }

    exec_buffer[rwcount++] = value;

    // More data can be written, just go on with the current buffer
    if (rwcount != rwcount_max)
        return 0;

    /* Commit the current buffer and prepare the next one to be written */
    uint8_t sector = rw_args->record;
    // FDC counts sectors from 1
    CEDA_STRONG_ASSERT_TRUE(sector != 0);
    // But all other routines counts sectors from 0
    sector--;
    int ret =
        write_buffer_cb(exec_buffer, drive, rw_args->unit_head & FDC_ST0_HD,
                        track[drive], rw_args->head, rw_args->cylinder, sector);
    // the image is always loaded and valid
    CEDA_STRONG_ASSERT_TRUE(ret > DISK_IMAGE_NOMEDIUM);
    // Buffer is statically allocated, be sure that the data can fit it
    CEDA_STRONG_ASSERT_TRUE((size_t)ret <= sizeof(exec_buffer));

    // Multi-sector mode (enabled by default).
    // If read is not interrupted at the end of the sector, the next logical
    // sector is loaded
    rw_args->record++;

    // Last sector of the track
    if (rw_args->record > rw_args->eot) {
        // In any case, reached the end of track we start back from sector 1
        rw_args->record = 1;

        // Multi track mode, if enabled the read operation go on on the next
        // side
        if (command_args & FDC_CMD_ARGS_MT_bm) {
            rw_args->unit_head ^= FDC_ST0_HD;
            rw_args->head = !rw_args->head;

            if (!(rw_args->unit_head & FDC_ST0_HD)) {
                // Terminate execution if end of track is reached
                tc_status = true;
                rw_args->cylinder++;
                return 0;
            }
        } else {
            // Terminate execution if end of track is reached
            tc_status = true;
            rw_args->cylinder++;
            return 0;
        }
    }

    buffer_write_size();

    return 0;
}

static void post_exec_write_data(void) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;

    LOG_DEBUG("Write has ended\n");

    memset(result, 0x00, sizeof(result));

    // TODO(giuliof): populate result as in datasheet (see table 2)
    /* ST0 */
    // Current head position
    result[0] |= drive;
    result[0] |= rw_args->unit_head & FDC_ST0_HD ? FDC_ST0_HD : 0;
    /* ST1 */
    result[1] |= 0; // TODO(giuliof): populate this
    /* ST2 */
    result[2] |= 0; // TODO(giuliof): populate this
    /* CHR */
    result[3] = rw_args->cylinder;
    result[4] = rw_args->head;
    result[5] = rw_args->record;
    /* Sector size factor */
    result[6] = rw_args->n;
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
    status_register |= FDC_ST_DIO;

    // TODO(giuliof) create handles to manage more than one floppy image at a
    // time
    // read_buffer_cb(exec_buffer, track_size, head, track, sector);
    // TODO(giuliof): may be a good idea to pass a sort of "floppy context"
    buffer_update();
}

static uint8_t exec_read_data(uint8_t value) {
    // read doesn't care of in value
    (void)value;

    rw_args_t *rw_args = (rw_args_t *)args;

    if (rwcount_max == 0) {
        LOG_WARN("Read execution happened when no data can be read");
        return 0;
    }

    uint8_t ret = exec_buffer[rwcount++];

    // More data can be read, just go on with the current buffer
    if (rwcount != rwcount_max)
        return ret;

    /* Prepare the next buffer to be read */
    // Multi-sector mode (enabled by default).
    // If read is not interrupted at the end of the sector, the next logical
    // sector is loaded
    rw_args->record++;

    // Last sector of the track
    if (rw_args->record > rw_args->eot) {
        // In any case, reached the end of track we start back from sector 1
        rw_args->record = 1;

        // Multi track mode, if enabled the read operation go on on the next
        // side
        if (command_args & FDC_CMD_ARGS_MT_bm) {
            rw_args->unit_head ^= FDC_ST0_HD;
            rw_args->head = !rw_args->head;

            if (!(rw_args->unit_head & FDC_ST0_HD)) {
                // Terminate execution if end of track is reached
                tc_status = true;
                rw_args->cylinder++;
                return 0;
            }
        } else {
            // Terminate execution if end of track is reached
            tc_status = true;
            rw_args->cylinder++;
            return 0;
        }
    }

    buffer_update();

    return ret;
}

static void post_exec_read_data(void) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;

    LOG_DEBUG("Read has ended\n");

    memset(result, 0x00, sizeof(result));

    // TODO(giuliof): populate result as in datasheet (see table 2)
    /* ST0 */
    // Current head position
    result[0] |= drive;
    result[0] |= rw_args->unit_head & FDC_ST0_HD ? FDC_ST0_HD : 0;
    /* ST1 */
    result[1] |= 0; // TODO(giuliof): populate this
    /* ST2 */
    result[2] |= 0; // TODO(giuliof): populate this
    /* CHR */
    result[3] = rw_args->cylinder;
    result[4] = rw_args->head;
    result[5] = rw_args->record;
    /* Sector size factor */
    result[6] = rw_args->n;
}

// Recalibrate:
// Just print the register values.
static void pre_exec_recalibrate(void) {
    uint8_t drive = args[0] & 0x3;

    LOG_DEBUG("FDC Recalibrate\n");
    LOG_DEBUG("Drive: %d\n", drive);

    track[drive] = 0;

    // We don't have to actually move the head. The drive is immediately ready
    int_status = true;
}

// Sense interrupt:
static void post_exec_sense_interrupt(void) {
    // TODO(giuliof): last accessed drive
    uint8_t drive = 0;

    // After reading interrupt status, ready can be deasserted
    int_status = false;

    LOG_DEBUG("FDC Sense Interrupt\n");
    /* Status Register 0 */
    // Drive number
    result[0] = drive;
    // head address (last addressed) - TODO(giulio)
    // result[0] |= ...;
    // Seek End - TODO(giulio)
    result[0] |= FDC_ST0_SE;
    /* PCN  - (current track position) */
    result[1] = track[drive];
}

// Format track
static void pre_exec_format_track(void) {
    format_args_t *format_args = (format_args_t *)args;

    // Extract plain data from the bitfield
    uint8_t phy_head = !!(format_args->unit_head & FDC_ST0_HD);
    uint8_t drive = format_args->unit_head & FDC_ST0_US;

    // TODO(giuliof): eventually: use the appropriate structure
    LOG_DEBUG("FDC Format track\n");
    LOG_DEBUG("MF: %d\n", (command_args >> 6) & 0x01);
    LOG_DEBUG("Drive: %d\n", args[0] & 0x3);
    LOG_DEBUG("HD: %d\n", (args[0] >> 2) & 0x1);
    LOG_DEBUG("N: %d\n", args[1]);
    LOG_DEBUG("SPT: %d\n", args[2]);
    LOG_DEBUG("GPL: %d\n", args[3]);
    LOG_DEBUG("D: %d\n", args[4]);

    // Initialize execution phase counter.
    // The FORMAT command requires the filling of a buffer of "ID fields", one
    // for each sector within the same track. Each "ID field" is 4 bytes long.
    // The number of sectors per track is specified by the command itself (SPT
    // argument).
    rwcount = 0;
    rwcount_max = (size_t)(format_args->sec_per_track * 4);
    assert(rwcount_max <= sizeof(exec_buffer));

    // check if the medium is present, else no irq is generated
    if (write_buffer_cb == NULL)
        return;

    // Check if medium is valid by poking sector 0 of the desired track
    // TODO(giuliof): add proper error code, zero is no mounted image
    int ret = write_buffer_cb(NULL, drive, phy_head, track[drive], phy_head,
                              track[drive], 0);

    if (ret <= DISK_IMAGE_NOMEDIUM)
        return;

    int_status = true;
}

static uint8_t exec_format_track(uint8_t value) {
    exec_buffer[rwcount++] = value;

    return 0;
}

static void post_exec_format_track(void) {
    format_args_t *format_args = (format_args_t *)args;

    // Extract plain data from the bitfield
    uint8_t phy_head = !!(format_args->unit_head & FDC_ST0_HD);
    uint8_t drive = format_args->unit_head & FDC_ST0_US;

    // At the moment the track format is just a writing over all "pre-formatted"
    // sectors. An arbitrary format is currently not supported.
    for (size_t s = 0; s < format_args->sec_per_track; s++) {
        uint8_t *id_field = exec_buffer + (4 * s);
        uint8_t cylinder = id_field[0];
        uint8_t head = id_field[1];
        uint8_t record = id_field[2] - 1;

        int ret = write_buffer_cb(NULL, drive, phy_head, track[drive], head,
                                  cylinder, record);
        CEDA_STRONG_ASSERT_TRUE(ret > DISK_IMAGE_NOMEDIUM);

        uint8_t format_buffer[ret];
        memset(format_buffer, format_args->d, (size_t)ret);

        ret = write_buffer_cb(format_buffer, drive, phy_head, track[drive],
                              head, cylinder, record);
        CEDA_STRONG_ASSERT_TRUE(ret > DISK_IMAGE_NOMEDIUM);
    }

    LOG_DEBUG("FDC end Format track\n");

    memset(result, 0, sizeof(result));
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
    int_status = true;
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
        status_register &= (uint8_t)~FDC_ST_DIO;

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
        tc_status = false;
        // Set DIO to read for RESULT phase
        status_register |= FDC_ST_DIO;

        if (fdc_currop->post_exec)
            fdc_currop->post_exec();

        fdc_status = RESULT;
        rwcount_max = fdc_currop->result_len;
        rwcount = 0;
    }

    if (fdc_status == RESULT && rwcount == rwcount_max) {
        // Set DIO to write for CMD and ARGS phases
        status_register &= (uint8_t)~FDC_ST_DIO;

        fdc_status = CMD;
        rwcount_max = 0;
        rwcount = 0;
    }

    // Update step dependant bits in main status register
    if (fdc_status == EXEC)
        status_register |= FDC_ST_EXM;
    else
        status_register &= (uint8_t)~FDC_ST_EXM;

    if (fdc_status != CMD)
        status_register |= FDC_ST_CB;
    else
        status_register &= (uint8_t)~FDC_ST_CB;
}

static void buffer_update(void) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;

    uint8_t sector = rw_args->record;

    // Default: no data ready to be served
    int_status = false;
    rwcount_max = 0;

    // FDC counts sectors from 1
    assert(sector != 0);

    // But all other routines counts sectors from 0
    sector--;

    if (read_buffer_cb == NULL)
        return;

    // TODO(giuliof): add proper error code, zero is no mounted image
    int ret =
        read_buffer_cb(NULL, drive, rw_args->unit_head & FDC_ST0_HD,
                       track[drive], rw_args->head, rw_args->cylinder, sector);

    if (ret != DISK_IMAGE_NOMEDIUM) {
        // TODO(giuliof): At the moment we do not support error codes, we assume
        // the image is always loaded and valid
        CEDA_STRONG_ASSERT_TRUE(ret > DISK_IMAGE_NOMEDIUM);
        // Buffer is statically allocated, be sure that the data can fit it
        CEDA_STRONG_ASSERT_TRUE((size_t)ret <= sizeof(exec_buffer));

        ret = read_buffer_cb(exec_buffer, drive,
                             rw_args->unit_head & FDC_ST0_HD, track[drive],
                             rw_args->head, rw_args->cylinder, sector);
        // TODO(giuliof): At the moment we do not support error codes, we assume
        // the image is always loaded and valid
        CEDA_STRONG_ASSERT_TRUE(ret > DISK_IMAGE_NOMEDIUM);

        // Ready to serve data
        int_status = true;
    }

    rwcount = 0;
    if (rw_args->n == 0)
        rwcount_max = MIN((size_t)rw_args->stp, (size_t)ret);
    else
        rwcount_max = (size_t)ret;
}

static void buffer_write_size(void) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;

    uint8_t sector = rw_args->record;

    // Default: no data ready to be served
    int_status = false;
    rwcount_max = 0;

    // FDC counts sectors from 1
    assert(sector != 0);

    // But all other routines counts sectors from 0
    sector--;

    if (write_buffer_cb == NULL)
        return;

    int ret =
        write_buffer_cb(NULL, drive, rw_args->unit_head & FDC_ST0_HD,
                        track[drive], rw_args->head, rw_args->cylinder, sector);

    // TODO(giuliof): At the moment we do not support error codes, we assume the
    // image is always loaded and valid
    CEDA_STRONG_ASSERT_TRUE(ret > DISK_IMAGE_NOMEDIUM);
    // Buffer is statically allocated, be sure that the data can fit it
    CEDA_STRONG_ASSERT_TRUE((size_t)ret <= sizeof(exec_buffer));

    rwcount = 0;
    if (rw_args->n == 0)
        rwcount_max = MIN((size_t)rw_args->stp, (size_t)ret);
    else
        rwcount_max = (size_t)ret;
    int_status = true;
}

/* * * * * * * * * * * * * * *  Public routines   * * * * * * * * * * * * * * */

void fdc_init(void) {
    // Reset current command status
    fdc_status = CMD;
    fdc_currop = NULL;

    // Reset any internal status
    rwcount_max = 0;
    memset(result, 0, sizeof(result));
    tc_status = false;
    int_status = false;

    // Reset main status register, but keep RQM active since FDC is always ready
    // to receive requests
    status_register = FDC_ST_RQM;

    // Reset track positions
    memset(track, 0, sizeof(track));

    // Detach any read/write callback
    read_buffer_cb = NULL;
    write_buffer_cb = NULL;
}

uint8_t fdc_in(ceda_ioaddr_t address) {
    switch (address & 0x01) {
    case FDC_ADDR_STATUS_REGISTER:
        return status_register;
    case FDC_ADDR_DATA_REGISTER: {
        uint8_t value = 0;

        if (fdc_status == CMD) {
            // You should never read when in CMD status.
            // Just reply with "invalid command" and restore data direction
            value = 0x80;
            status_register &= (uint8_t)~FDC_ST_DIO;
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
    case FDC_ADDR_STATUS_REGISTER:
        LOG_WARN("nobody should write in FDC main status register\n");
        return;
    case FDC_ADDR_DATA_REGISTER: {
        if (fdc_status == CMD) {
            // Split the command itself from option bits
            uint8_t cmd = value & FDC_CMD_COMMAND_bm;
            command_args = value & FDC_CMD_ARGS_bm;

            // Unroll the command list and place it in the current execution
            // But ignore command if in interrupt status (after seek or
            // recalibrate) and next command is not sense interrupt.
            // In this case, the command is treated as invalid.
            fdc_currop = NULL;
            if (!(int_status && cmd != FDC_SENSE_INTERRUPT)) {
                for (size_t i = 0;
                     i < sizeof(fdc_operations) / sizeof(*fdc_operations);
                     i++) {
                    if (cmd == fdc_operations[i].cmd) {
                        fdc_currop = &fdc_operations[i];
                        break;
                    }
                }
            }
            if (fdc_currop == NULL) {
                LOG_WARN("Command %x is not implemented\n", cmd);

                // Invalid command: set the main status register to read to
                // serve ST0 and error code
                status_register |= FDC_ST_DIO;
            }
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
        tc_status = true;
        fdc_compute_next_status();
    }
}

// TODO(giuliof): After Execution Phase or EOR sector read, INT=1
// (beginning of result phase). When first byte of result phase data
// is read, INT=0.
bool fdc_getIntStatus(void) {
    return int_status;
}

// TODO(giuliof): describe better this function
// Fast notes: if an image is loaded at runtime, check if the code is stuck in
// a read or write loop that was waiting for interrupt.
// In that case, remove the lock (buffer_update will do it, for the writing it
// has to be implemented)
void fdc_kickDiskImage(fdc_read_write_t read_callback,
                       fdc_read_write_t write_callback) {
    read_buffer_cb = read_callback;
    write_buffer_cb = write_callback;

    if (fdc_status == EXEC && fdc_currop->cmd == FDC_READ_DATA) {
        buffer_update();
    }

    if (fdc_status == EXEC && fdc_currop->cmd == FDC_WRITE_DATA) {
        buffer_write_size();
    }
}
