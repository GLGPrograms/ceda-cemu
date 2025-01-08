#ifndef CEDA_FDC_REGISTERS_H_
#define CEDA_FDC_REGISTERS_H_

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

#endif // CEDA_FDC_REGISTERS_H_