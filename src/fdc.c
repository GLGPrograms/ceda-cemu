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
    uint8_t dtl;
} rw_args_t;

// Parsing structure for format arguments
typedef struct format_args_t {
    uint8_t unit_head;
    uint8_t n;             // bytes per sector factor
    uint8_t sec_per_track; // number of sectors per track
    uint8_t gpl;           // gap length
    uint8_t d;             // filler byte
} format_args_t;

// ID Register, tracks the current ID during rw operations
typedef struct idr_t {
    uint8_t phy_head;
    uint8_t cylinder;
    uint8_t head;
    uint8_t record;
} idr_t;

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
static bool is_cmd_out_of_sequence(uint8_t cmd);
static void fdc_compute_next_status(void);
static void set_invalid_cmd(void);
static bool fdc_prepare_read(void);
static bool fdc_commit_write(void);

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
// This is a dummy operation used when an invalid command has to be handled
static const fdc_operation_t invalid_op = {
    .cmd = 0x00, // This command code does't exists
    .args_len = 0,
    .result_len = 1, // Invalid command has to report just ST0
    .pre_exec = NULL,
    .exec = NULL,
    .post_exec = NULL,
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
enum { MSR, ST0, ST1, ST2, ST3, NUM_OF_SREG };
// Main Status Register
static uint8_t status_register[NUM_OF_SREG];

/* Floppy disk status */
// Current track position
static uint8_t track[4];

/* Callbacks to handle floppy read and write */
static fdc_read_write_t read_buffer_cb = NULL;
static fdc_read_write_t write_buffer_cb = NULL;

/* ID Register, store CHR for the current and the next record under execution */
idr_t idr, next_idr;

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
    rw_args_t *rw_args = (rw_args_t *)args;

    LOG_DEBUG("FDC Write Data\n");
    LOG_DEBUG("MF: %d\n", !!(command_args & FDC_CMD_ARGS_MF_bm));
    LOG_DEBUG("MT: %d\n", !!(command_args & FDC_CMD_ARGS_MT_bm));
    LOG_DEBUG("Drive: %d\n", rw_args->unit_head & FDC_ST0_US);
    LOG_DEBUG("HD: %d\n", !!(rw_args->unit_head & FDC_ST0_HD));
    LOG_DEBUG("Cyl: %d\n", rw_args->cylinder);
    LOG_DEBUG("Head: %d\n", rw_args->head);
    LOG_DEBUG("Record: %d\n", rw_args->record);
    LOG_DEBUG("N: %d\n", rw_args->n);
    LOG_DEBUG("EOT: %d\n", rw_args->eot);
    LOG_DEBUG("GPL: %d\n", rw_args->gpl);
    LOG_DEBUG("DTL: %d\n", rw_args->dtl);

    // Set DIO to read for Execution phase
    status_register[MSR] &= (uint8_t)~FDC_ST_DIO;

    idr.phy_head = rw_args->unit_head;
    idr.cylinder = rw_args->cylinder;
    idr.head = rw_args->head;
    idr.record = rw_args->record;
    memcpy(&next_idr, &idr, sizeof(idr));

    fdc_commit_write();
}

static uint8_t exec_write_data(uint8_t value) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;

    if (write_buffer_cb == NULL)
        return 0;

    if (rwcount >= rwcount_max) {
        if (!fdc_commit_write())
            return 0;
    }

    exec_buffer[rwcount++] = value;
    // From the manual: in NON-DMA mode, interrupt is generated during
    // execution phase (as soon as new data is available)
    int_status = true;

    // More data can be written, just go on with the current buffer
    if (rwcount != rwcount_max)
        return 0;

    /* Commit the current buffer and prepare the next one to be written */
    uint8_t sector = idr.record;
    // FDC counts sectors from 1
    CEDA_STRONG_ASSERT_TRUE(sector != 0);
    // But all other routines counts sectors from 0
    sector--;
    int ret = write_buffer_cb(exec_buffer, drive, idr.phy_head & FDC_ST0_HD,
                              track[drive], idr.head, idr.cylinder, sector);

    // Error condition
    // TODO(giuliof): errors may be differentiated, but for the moment cath all
    // as a generic error -- even if no medium because it's too late to handle
    // by pausing execution!
    if (ret <= DISK_IMAGE_NOMEDIUM) {
        LOG_WARN("Reading error occurred, code %d\n", ret);
        // Update status register setting error condition and error type flags
        status_register[ST0] |= 0x40;
        status_register[ST1] |= 0x20;
        status_register[ST2] |= 0x20;
        // Execution is terminated after an error
        tc_status = true;
        // Force an interrupt
        int_status = true;
        return 0;
    }

    return 0;
}

static void post_exec_write_data(void) {
    rw_args_t *rw_args = (rw_args_t *)args;

    LOG_DEBUG("Write has ended\n");

    memset(result, 0x00, sizeof(result));

    /* Status registers 0-2 */
    result[0] = status_register[ST0];
    result[1] = status_register[ST1];
    result[2] = status_register[ST2];
    /* CHR */
    // When the FDC exits from exec mode with no error, next IDR should be used
    if ((result[0] & FDC_ST0_IC) == 0) {
        result[0] &= (uint8_t)~FDC_ST0_HD;
        if (next_idr.phy_head & FDC_ST0_HD)
            result[0] |= FDC_ST0_HD;
        result[3] = next_idr.cylinder;
        result[4] = next_idr.head;
        result[5] = next_idr.record;
    }
    // Else, in case of error, use the last valid read sector (current IDR)
    else {
        result[0] &= (uint8_t)~FDC_ST0_HD;
        if (idr.phy_head & FDC_ST0_HD)
            result[0] |= FDC_ST0_HD;
        result[3] = idr.cylinder;
        result[4] = idr.head;
        result[5] = idr.record;
    }
    /* Sector size factor */
    result[6] = rw_args->n;
}

// Read data:
static void pre_exec_read_data(void) {
    rw_args_t *rw_args = (rw_args_t *)args;

    LOG_DEBUG("FDC Read Data\n");
    LOG_DEBUG("SK: %d\n", !!(command_args & FDC_CMD_ARGS_SK_bm));
    LOG_DEBUG("MF: %d\n", !!(command_args & FDC_CMD_ARGS_MF_bm));
    LOG_DEBUG("MT: %d\n", !!(command_args & FDC_CMD_ARGS_MT_bm));
    LOG_DEBUG("Drive: %d\n", rw_args->unit_head & FDC_ST0_US);
    LOG_DEBUG("HD: %d\n", !!(rw_args->unit_head & FDC_ST0_HD));
    LOG_DEBUG("Cyl: %d\n", rw_args->cylinder);
    LOG_DEBUG("Head: %d\n", rw_args->head);
    LOG_DEBUG("Record: %d\n", rw_args->record);
    LOG_DEBUG("N: %d\n", rw_args->n);
    LOG_DEBUG("EOT: %d\n", rw_args->eot);
    LOG_DEBUG("GPL: %d\n", rw_args->gpl);
    LOG_DEBUG("DTL: %d\n", rw_args->dtl);

    // Set DIO to read for Execution phase
    status_register[MSR] |= FDC_ST_DIO;

    idr.phy_head = rw_args->unit_head;
    idr.cylinder = rw_args->cylinder;
    idr.head = rw_args->head;
    idr.record = rw_args->record;
    memcpy(&next_idr, &idr, sizeof(idr));

    // TODO(giuliof): may be a good idea to pass a sort of "floppy context"
    fdc_prepare_read();
}

static uint8_t exec_read_data(uint8_t value) {
    // read doesn't care of in value
    (void)value;

    uint8_t ret = 0;

    // Sector buffer already populated and on-going reading
    if (rwcount < rwcount_max) {
        ret = exec_buffer[rwcount++];
    }
    // No sector buffer or finished one, try to get another sector from image
    else if ((rwcount_max == 0 || rwcount >= rwcount_max)) {
        if (fdc_prepare_read())
            ret = exec_buffer[rwcount++];
    }

    // From the manual: in NON-DMA mode, interrupt is generated during
    // execution phase (as soon as new data is available)
    int_status = true;

    /* Prepare the next buffer to be read */
    return ret;
}

static void post_exec_read_data(void) {
    rw_args_t *rw_args = (rw_args_t *)args;

    LOG_DEBUG("Read has ended\n");

    /* Status registers 0-2 */
    result[0] = status_register[ST0];
    result[1] = status_register[ST1];
    result[2] = status_register[ST2];
    /* CHR */
    // When the FDC exits from exec mode with no error, next IDR should be used
    if ((result[0] & FDC_ST0_IC) == 0) {
        result[0] &= (uint8_t)~FDC_ST0_HD;
        if (next_idr.phy_head & FDC_ST0_HD)
            result[0] |= FDC_ST0_HD;
        result[3] = next_idr.cylinder;
        result[4] = next_idr.head;
        result[5] = next_idr.record;
    }
    // Else, in case of error, use the last valid read sector (current IDR)
    else {
        result[0] &= (uint8_t)~FDC_ST0_HD;
        if (idr.phy_head & FDC_ST0_HD)
            result[0] |= FDC_ST0_HD;
        result[3] = idr.cylinder;
        result[4] = idr.head;
        result[5] = idr.record;
    }
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
    // Update the status register with the drive info and the seek end flag
    status_register[ST0] = drive;
    // Update the FDD n busy flag, will be cleared by sense interrupt
    status_register[MSR] |= (1 << drive);
}

// Sense interrupt:
static void post_exec_sense_interrupt(void) {
    LOG_DEBUG("FDC Sense Interrupt\n");

    // Get the last "seeked" drive number from the MSR
    uint8_t fdc_busy = status_register[MSR] &
                       (FDC_ST_D3B | FDC_ST_D2B | FDC_ST_D1B | FDC_ST_D0B);
    // This routine should be called only if fdc is busy
    assert(fdc_busy != 0);
    uint8_t drive = 0;
    for (; (fdc_busy & (1 << drive)) == 0; drive++)
        ;

    // Deassert busy state and eventually retrigger INT (TODO: verify)
    status_register[MSR] &= (uint8_t) ~(1 << drive);
    if (status_register[MSR] &
        (FDC_ST_D3B | FDC_ST_D2B | FDC_ST_D1B | FDC_ST_D0B))
        int_status = true;

    /* Status Register 0 */
    result[0] = status_register[ST0] | FDC_ST0_SE;
    /* PCN  - (current track position) */
    result[1] = track[drive];
}

// Format track
static void pre_exec_format_track(void) {
    format_args_t *format_args = (format_args_t *)args;

    // Extract plain data from the bitfield
    uint8_t phy_head = !!(format_args->unit_head & FDC_ST0_HD);
    uint8_t drive = format_args->unit_head & FDC_ST0_US;

    LOG_DEBUG("FDC Format track\n");
    LOG_DEBUG("MF: %d\n", !!(command_args & FDC_CMD_ARGS_MF_bm));
    LOG_DEBUG("Drive: %d\n", format_args->unit_head & FDC_ST0_US);
    LOG_DEBUG("HD: %d\n", !!(format_args->unit_head & FDC_ST0_HD));
    LOG_DEBUG("N: %d\n", format_args->n);
    LOG_DEBUG("SPT: %d\n", format_args->sec_per_track);
    LOG_DEBUG("GPL: %d\n", format_args->gpl);
    LOG_DEBUG("D: %d\n", format_args->d);

    // Set deafult values of status registers
    status_register[ST0] = format_args->unit_head;
    status_register[ST1] = 0;
    status_register[ST2] = 0;
    status_register[ST3] = 0;

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
    int ret = write_buffer_cb(NULL, drive, phy_head, track[drive], phy_head,
                              track[drive], 0);

    if (ret <= DISK_IMAGE_NOMEDIUM) {
        LOG_WARN("Format error occurred, code %d\n", ret);
        // Update status register setting error condition and error type
        // flags
        status_register[ST0] |= 0x40;
        status_register[ST1] |= 0x20;
        status_register[ST2] |= 0x20;
        // Execution is terminated after an error
        tc_status = true;
    }

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

        if (ret > DISK_IMAGE_NOMEDIUM) {
            uint8_t format_buffer[ret];
            memset(format_buffer, format_args->d, (size_t)ret);

            ret = write_buffer_cb(format_buffer, drive, phy_head, track[drive],
                                  head, cylinder, record);
        }

        if (ret <= DISK_IMAGE_NOMEDIUM) {
            LOG_WARN("Format error occurred, code %d\n", ret);
            // Update status register setting error condition and error type
            // flags
            status_register[ST0] |= 0x40;
            status_register[ST1] |= 0x20;
            status_register[ST2] |= 0x20;
            // Execution is terminated after an error
            tc_status = true;
            // Force an interrupt
            int_status = true;
        }
    }

    LOG_DEBUG("FDC end Format track\n");

    memset(result, 0, sizeof(result));

    /* Status registers 0-2 */
    result[0] |= status_register[ST0];
    result[1] |= status_register[ST1];
    result[2] |= status_register[ST2];
    /* CHR and N have no meaning */
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
    // Update the status register with the drive info and the seek end flag
    status_register[ST0] = drive;
    // Update the FDD n busy flag, will be cleared by sense interrupt
    status_register[MSR] |= (1 << drive);
}

/* * * * * * * * * * * * * * *  Utility routines  * * * * * * * * * * * * * * */

static bool is_cmd_out_of_sequence(uint8_t cmd) {
    bool fdc_busy = status_register[MSR] &
                    ((FDC_ST_D3B | FDC_ST_D2B | FDC_ST_D1B | FDC_ST_D0B));

    if (cmd == FDC_SEEK || cmd == FDC_RECALIBRATE)
        return false;
    // TODO: to be correct, I should check int, but it was already
    // cleared in command read/write routine
    else if (cmd == FDC_SENSE_INTERRUPT) {
        if (fdc_busy)
            return false;
    }
    //
    else {
        if (!fdc_busy)
            return false;
    }

    return true;
}

static void fdc_compute_next_status(void) {
    if (!fdc_currop)
        return;

    // rwcount during execution phase is handled directly by the exec callback
    if (fdc_status != EXEC)
        rwcount++;

    if (fdc_status == CMD) {
        // Set DIO to write for ARGS phase
        status_register[MSR] &= (uint8_t)~FDC_ST_DIO;

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
        status_register[MSR] |= FDC_ST_DIO;

        if (fdc_currop->post_exec)
            fdc_currop->post_exec();

        fdc_status = RESULT;
        rwcount_max = fdc_currop->result_len;
        rwcount = 0;
    }

    if (fdc_status == RESULT && rwcount == rwcount_max) {
        // Set DIO to write for CMD and ARGS phases
        status_register[MSR] &= (uint8_t)~FDC_ST_DIO;

        fdc_status = CMD;
        rwcount_max = 0;
        rwcount = 0;
    }

    // Update step dependant bits in main status register
    if (fdc_status == EXEC)
        status_register[MSR] |= FDC_ST_EXM;
    else
        status_register[MSR] &= (uint8_t)~FDC_ST_EXM;

    if (fdc_status != CMD)
        status_register[MSR] |= FDC_ST_CB;
    else
        status_register[MSR] &= (uint8_t)~FDC_ST_CB;
}

static void set_invalid_cmd(void) {
    // Invalid command is not an actual command...
    fdc_currop = &invalid_op;

    fdc_status = CMD;

    // TODO(giuliof): current drive has to be preserved?
    status_register[ST0] &= (uint8_t)~FDC_ST0_US;
    // TODO(giuliof): should present other flags?
    status_register[ST0] |= 0x80;

    // Immediately prepare result
    result[0] = status_register[ST0];
}

/**
 * @brief This helper routine fetches the read buffer from the disk image, to
 * be served during execution phase. It updates the status registers too, moving
 * head/track/sector pointers.
 *
 * @return false on failure
 */
static bool fdc_prepare_read(void) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;
    uint8_t sector = next_idr.record;

    rwcount = 0;
    rwcount_max = 0;
    status_register[ST0] = drive;
    status_register[ST1] = 0;
    status_register[ST2] = 0;
    status_register[ST3] = 0;

    // FDC counts sectors from 1
    assert(sector != 0);

    // But all other routines counts sectors from 0
    sector--;

    if (read_buffer_cb == NULL)
        return false;

    int ret =
        read_buffer_cb(NULL, drive, next_idr.phy_head & FDC_ST0_HD,
                       track[drive], next_idr.head, next_idr.cylinder, sector);

    if (ret > DISK_IMAGE_NOMEDIUM) {
        // Buffer is statically allocated, be sure that the data can fit it
        CEDA_STRONG_ASSERT_TRUE((size_t)ret <= sizeof(exec_buffer));

        ret = read_buffer_cb(exec_buffer, drive, next_idr.phy_head & FDC_ST0_HD,
                             track[drive], next_idr.head, next_idr.cylinder,
                             sector);
    }

    // No medium, FDC is in EXEC state until a disk is inserted, or manual
    // termination
    if (ret == DISK_IMAGE_NOMEDIUM)
        return false;

    // generate interrupt since an event has occurred
    int_status = true;

    // Ready to serve data
    if (ret > DISK_IMAGE_NOMEDIUM) {
        if (rw_args->n == 0)
            rwcount_max = MIN((size_t)rw_args->dtl, (size_t)ret);
        else
            rwcount_max = (size_t)ret;

        // Update IDR for the next sector to be read
        // TODO(giuliof): This can be done in a function

        // Confirm the current IDR, since the buffer update was successful
        memcpy(&idr, &next_idr, sizeof(idr));

        // Multi-sector mode (enabled by default).
        // If read is not interrupted at the end of the sector, the next logical
        // sector is loaded
        next_idr.record++;

        // Last sector of the track
        if (next_idr.record > rw_args->eot) {
            // In any case, reached the end of track we start back from sector 1
            next_idr.record = 1;

            // Multi track mode, if enabled the read operation go on on the next
            // side
            if (command_args & FDC_CMD_ARGS_MT_bm) {
                next_idr.phy_head ^= FDC_ST0_HD;
                next_idr.head = !next_idr.head;

                if (!(next_idr.phy_head & FDC_ST0_HD))
                    next_idr.cylinder++;

            } else {
                next_idr.cylinder++;
            }
        }

        return true;
    }
    // Error condition
    // TODO(giuliof): errors may be differentiated, but for the moment cath all
    // as a generic error
    LOG_WARN("Reading error occurred, code %d\n", ret);
    // Update status register setting error condition and error type flags
    status_register[ST0] |= 0x40;
    status_register[ST1] |= 0x20;
    status_register[ST2] |= 0x20;
    // Execution is terminated after an error
    tc_status = true;
    return false;
}

/**
 * @brief This helper routine writes the buffer, populated during the execution
 * phase, into the disk image. It updates the status registers too, moving
 * head/track/sector pointers.
 *
 * @return false on failure
 */
static bool fdc_commit_write(void) {
    rw_args_t *rw_args = (rw_args_t *)args;
    uint8_t drive = rw_args->unit_head & FDC_ST0_US;
    uint8_t sector = next_idr.record;

    rwcount = 0;
    rwcount_max = 0;
    status_register[ST0] = drive;
    status_register[ST1] = 0;
    status_register[ST2] = 0;
    status_register[ST3] = 0;

    // FDC counts sectors from 1
    assert(sector != 0);

    // But all other routines counts sectors from 0
    sector--;

    if (write_buffer_cb == NULL)
        return false;

    int ret =
        write_buffer_cb(NULL, drive, next_idr.phy_head & FDC_ST0_HD,
                        track[drive], next_idr.head, next_idr.cylinder, sector);

    // No medium, FDC is in EXEC state until a disk is inserted, or manual
    // termination
    if (ret == DISK_IMAGE_NOMEDIUM)
        return false;

    // generate interrupt since an event has occurred
    int_status = true;

    // Ready to serve data
    if (ret > DISK_IMAGE_NOMEDIUM) {
        if (rw_args->n == 0)
            rwcount_max = MIN((size_t)rw_args->dtl, (size_t)ret);
        else
            rwcount_max = (size_t)ret;

        // Update IDR for the next sector to be read
        // TODO(giuliof): This can be done in a function

        // Confirm the current IDR, since the buffer update was successful
        memcpy(&idr, &next_idr, sizeof(idr));

        // Multi-sector mode (enabled by default).
        // If read is not interrupted at the end of the sector, the next logical
        // sector is loaded
        next_idr.record++;

        // Last sector of the track
        if (next_idr.record > rw_args->eot) {
            // In any case, reached the end of track we start back from sector 1
            next_idr.record = 1;

            // Multi track mode, if enabled the read operation go on on the next
            // side
            if (command_args & FDC_CMD_ARGS_MT_bm) {
                next_idr.phy_head ^= FDC_ST0_HD;
                next_idr.head = !next_idr.head;

                if (!(next_idr.phy_head & FDC_ST0_HD))
                    next_idr.cylinder++;

            } else {
                next_idr.cylinder++;
            }
        }

        return true;
    }
    // Error condition
    // TODO(giuliof): errors may be differentiated, but for the moment cath all
    // as a generic error
    LOG_WARN("Reading error occurred, code %d\n", ret);
    // Update status register setting error condition and error type flags
    status_register[ST0] |= 0x40;
    status_register[ST1] |= 0x20;
    status_register[ST2] |= 0x20;
    // Execution is terminated after an error
    tc_status = true;

    return false;
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
    status_register[MSR] = FDC_ST_RQM;

    // Reset track positions
    memset(track, 0, sizeof(track));

    // Detach any read/write callback
    read_buffer_cb = NULL;
    write_buffer_cb = NULL;
}

uint8_t fdc_in(ceda_ioaddr_t address) {
    // The interrupt is cleared by reading/writing data to the FDC
    int_status = false;

    switch (address & 0x01) {
    case FDC_ADDR_STATUS_REGISTER:
        return status_register[MSR];
    case FDC_ADDR_DATA_REGISTER: {
        uint8_t value = 0;

        if (fdc_status == CMD) {
            // You should never read when in CMD status.
            // Just reply with ST0.
            value = status_register[ST0];

            status_register[MSR] &= (uint8_t)~FDC_ST_DIO;
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
    // The interrupt is cleared by reading/writing data to the FDC
    int_status = false;

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
            if (!is_cmd_out_of_sequence(cmd)) {
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

                // Invalid command is not an actual command...
                fdc_currop = &invalid_op;

                set_invalid_cmd();
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
        tc_status = true;
        fdc_compute_next_status();
    }
}

bool fdc_getIntStatus(void) {
    return int_status;
}

// TODO(giuliof): describe better this function
// Fast notes: if an image is loaded at runtime, check if the code is stuck in
// a read or write loop that was waiting for interrupt.
// In that case, remove the lock (fdc_prepare_read will do it, for the writing
// it has to be implemented)
void fdc_kickDiskImage(fdc_read_write_t read_callback,
                       fdc_read_write_t write_callback) {
    read_buffer_cb = read_callback;
    write_buffer_cb = write_callback;

    if (fdc_status == EXEC && fdc_currop->cmd == FDC_READ_DATA) {
        fdc_prepare_read();
    }

    if (fdc_status == EXEC && fdc_currop->cmd == FDC_WRITE_DATA) {
        fdc_commit_write();
    }
}
