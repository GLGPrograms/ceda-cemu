#ifndef CEDA_FDC_REGISTERS_H_
#define CEDA_FDC_REGISTERS_H_

// The FDC virtually expose two registers, which can be both read or written
#define FDC_ADDR_STATUS_REGISTER (0x00)
#define FDC_ADDR_DATA_REGISTER   (0x01)

// The FDC can perform 15 different commands.
// Don't ask me why they didn't use a single nibble to represent all the
// commands. Please refer to the datasheet for more information.
typedef enum fdc_cmd_t {
    INVALID = 0x00,
    FDC_READ_TRACK = 0x02,
    FDC_SPECIFY = 0x03,
    FDC_SENSE_DRIVE = 0x04,
    FDC_WRITE_DATA = 0x05,
    FDC_READ_DATA = 0x06,
    FDC_RECALIBRATE = 0x07,
    FDC_SENSE_INTERRUPT = 0x08,
    FDC_WRITE_DELETED_DATA = 0x09,
    FDC_READ_ID = 0x0A,
    FDC_READ_DELETED_DATA = 0x0C,
    FDC_FORMAT_TRACK = 0x0D,
    FDC_SEEK = 0x0F,
    FDC_SCAN_EQUAL = 0x11,
    FDC_SCAN_LOW_EQUAL = 0x19,
    FDC_SCAN_HIGH_EQUAL = 0x1D
} fdc_cmd_t;

// Some commands carry argument bits in MSb
#define FDC_CMD_COMMAND_bm (0x1F)
#define FDC_CMD_ARGS_bm    (0xE0)
#define FDC_CMD_ARGS_MT_bm (0x80)
#define FDC_CMD_ARGS_MF_bm (0x40)
#define FDC_CMD_ARGS_SK_bm (0x20)

/* Main status register bitfield */
// Request From Master, set if FDC is ready to receive or send data
#define FDC_ST_RQM (1 << 7)
// Data I/O, set if FDC is read from CPU, clear otherwise
#define FDC_ST_DIO (1 << 6)
// Execution mode
#define FDC_ST_EXM (1 << 5)
// Controller has already accepted a command
#define FDC_ST_CB (1 << 4)
// Drive x is in Seek mode
#define FDC_ST_D3B (1 << 3)
#define FDC_ST_D2B (1 << 2)
#define FDC_ST_D1B (1 << 1)
#define FDC_ST_D0B (1 << 0)

/* Status Register 0 bitfield */
#define FDC_ST0_IC (0xC0)
// seek end
#define FDC_ST0_SE (1 << 5)
// equipment check: fault signal from FDD or recalibrate failure
#define FDC_ST0_EC (1 << 4)
// not ready
#define FDC_ST0_NR (1 << 3)
// state of the head
#define FDC_ST0_HD (1 << 2)
// drive unit number at interrupt
#define FDC_ST0_US (0x03)

/* Status Register 1 bitfield */
#define FDC_ST1_EN (1 << 7)
#define FDC_ST1_DE (1 << 5)
#define FDC_ST1_OR (1 << 4)
#define FDC_ST1_ND (1 << 2)
#define FDC_ST1_NW (1 << 1)
#define FDC_ST1_NA (1 << 0)

/* Status Register 2 bitfield */
#define FDC_ST2_CM (1 << 6)
#define FDC_ST2_DD (1 << 5)
#define FDC_ST2_WC (1 << 4)
#define FDC_ST2_SH (1 << 3)
#define FDC_ST2_SN (1 << 2)
#define FDC_ST2_BC (1 << 1)
#define FDC_ST2_MD (1 << 0)

/* Status Register 3 bitfield */
#define FDC_ST3_FT (1 << 7)
#define FDC_ST3_WP (1 << 6)
#define FDC_ST3_RY (1 << 5)
#define FDC_ST3_T0 (1 << 4)
#define FDC_ST3_TS (1 << 3)
#define FDC_ST3_HD (1 << 2)
#define FDC_ST3_US (0x03)

#endif // CEDA_FDC_REGISTERS_H_