
#include <criterion/criterion.h>
#include <criterion/parameterized.h>

// TODO source path is src!
#include "../fdc.h"
#include "../fdc_registers.h"

static void assert_fdc_sr(uint8_t expected_sr);
static int fake_read(uint8_t *buffer, uint8_t unit_number, bool phy_head,
                     uint8_t phy_track, bool head, uint8_t track,
                     uint8_t sector);
static int fake_write(uint8_t *buffer, uint8_t unit_number, bool phy_head,
                      uint8_t phy_track, bool head, uint8_t track,
                      uint8_t sector);

/**
 * @brief Helper function to check the current status of the FDC main status
 * register
 *
 * @param expected_sr the expected value of the FDC main status register
 */
static void assert_fdc_sr(uint8_t expected_sr) {
    uint8_t sr;
    sr = fdc_in(FDC_ADDR_STATUS_REGISTER);
    // cr_log_info("%x != %x", sr, expected_sr);
    cr_expect_eq(sr, expected_sr);
}

static int fake_read(uint8_t *buffer, uint8_t unit_number, bool phy_head,
                     uint8_t phy_track, bool head, uint8_t track,
                     uint8_t sector) {
    (void)buffer;
    (void)unit_number;
    (void)phy_head;
    (void)phy_track;
    (void)head;
    (void)track;
    (void)sector;

    return 4;
}

Test(ceda_fdc, mainStatusRegisterWhenIdle) {
    fdc_init();

    // Try to read status register and check that it is idle
    assert_fdc_sr(FDC_ST_RQM);
}

Test(ceda_fdc, specifyCommand) {
    fdc_init();

    // Try to read status register and check that it is idle
    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_SPECIFY);

    // Now read status register to check that FDC is ready to receive arguments
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);

    // Pass dummy arguments
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);

    // FDC is no more busy
    assert_fdc_sr(FDC_ST_RQM);
}

Test(ceda_fdc, senseInterruptStatusCommand) {
    fdc_init();

    uint8_t data;

    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_SENSE_INTERRUPT);

    // This command has no arguments
    // FDC should be ready to give response
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_CB);

    // First response byte is SR0 with interrupt code = 0 and Seek End = 1
    data = fdc_in(FDC_ADDR_DATA_REGISTER);
    cr_expect_eq(data, FDC_ST0_SE);

    // FDC has another byte of response
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_CB);

    // Second response byte is current cylinder, which should be zero at reset
    data = fdc_in(FDC_ADDR_DATA_REGISTER);
    cr_expect_eq(data, 0);
}

Test(ceda_fdc, seekCommand) {
    fdc_init();

    uint8_t data;

    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_SEEK);

    // Now read status register to check that FDC is ready to receive arguments
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);

    // First argument is number of drive
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    // Second argument is cylinder position
    fdc_out(FDC_ADDR_DATA_REGISTER, 5);

    // FDC is no more busy
    assert_fdc_sr(FDC_ST_RQM);

    // A sense interrupt command is expected after FDC_SEEK
    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_SENSE_INTERRUPT);

    // This command has no arguments
    // FDC should be ready to give response
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_CB);

    // First response byte is SR0 with interrupt code = 0 and Seek End = 1
    data = fdc_in(FDC_ADDR_DATA_REGISTER);
    cr_expect_eq(data, FDC_ST0_SE);

    // FDC has another byte of response
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_CB);

    // Second response byte is current cylinder, which should be the one
    // specified by the seek argument
    data = fdc_in(FDC_ADDR_DATA_REGISTER);
    cr_expect_eq(data, 5);
}

/**
 * @brief Auxiliary function, send a data buffer to the FDC checking that it is
 * in input mode for each byte
 */
static void sendBuffer(const uint8_t *buffer, size_t size) {
    while (size-- > 0) {
        assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
        fdc_out(FDC_ADDR_DATA_REGISTER, *(buffer++));
    }
}

/**
 * @brief Auxiliary function, receive a data buffer from the FDC checking that
 * it is in output mode for each byte
 */
static void receiveBuffer(uint8_t *buffer, size_t size) {
    while (size-- > 0) {
        assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_CB);
        *(buffer++) = fdc_in(FDC_ADDR_DATA_REGISTER);
    }
}

Test(ceda_fdc, readCommandNoMedium) {
    uint8_t arguments[8] = {
        0, // drive number
        1, // cylinder
        0, // head
        1, // record
        1, // N - bytes per sector size factor
        5, // EOT (end of track)
        0, // GPL (ignored)
        0, // DTL (ignored)
    };

    fdc_init();

    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_READ_DATA);

    // Send arguments checking for no error
    sendBuffer(arguments, sizeof(arguments));

    // FDC switches IO mode, but...
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_EXM | FDC_ST_CB);
    // ... is not ready since no medium is loaded
    cr_assert_eq(fdc_getIntStatus(), false);

    // Kick medium in...
    fdc_kickDiskImage(fake_read, NULL);
    // ... now FDC is ready
    cr_assert_eq(fdc_getIntStatus(), true);
}

/**
 * @brief This section covers the cases described in table 2-2 of xxxxxxx
 * datasheet.
 * Please note that the FDC should accept logical head value different than
 * physical one.
 */

struct rw_test_params_t {
    uint8_t cmd_alteration;
    bool tc_required;
    uint8_t arguments[8];
    uint8_t result[7];
};

static struct rw_test_params_t rwparams[] = {
    {
        // No MT, end record < EOT, physical head 0
        0,    // No alteration
        true, // Terminal count required
        {
            0,  // drive number
            7,  // cylinder
            0,  // head
            5,  // record
            1,  // N - bytes per sector size factor
            10, // EOT (end of track)
            0,  // GPL (ignored)
            0,  // DTL (ignored)
        },
        {
            0, // Drive number, no error
            0, // no error
            0, // no error
            7, // cylinder
            0, // head
            7, // record
            1, // N
        },
    },
    {
        // No MT, end record = EOT, physical head 0
        0,
        false, // Terminal count not required
        {
            1,  // drive number
            7,  // cylinder
            1,  // head - different from physical head just for fun
            9,  // record
            1,  // N - bytes per sector size factor
            10, // EOT (end of track)
            0,  // GPL
            0,  // DTL
        },
        {
            1, // drive number, no error
            0, // no error
            0, // no error
            8, // cylinder
            1, // head
            1, // record
            1, // N
        },
    },
    {
        // No MT, end record < EOT, physical head 1
        0,
        true, // Terminal count required
        {
            FDC_ST0_HD | 2, // Drive number, physical head 1
            7,              // cylinder
            0,              // head - different from physical head just for fun
            5,              // record
            1,              // N - bytes per sector size factor
            10,             // EOT (end of track)
            0,              // GPL
            0,              // DTL
        },
        {
            FDC_ST0_HD | 2, // drive number, physical head 1, no error
            0,              // no error
            0,              // no error
            7,              // cylinder
            0,              // head
            7,              // record
            1,              // N
        },
    },
    {
        // No MT, end record = EOT, physical head 1
        0,
        false, // Terminal count not required
        {
            FDC_ST0_HD | 3, // Drive number, physical head 1
            7,              // cylinder
            1,              // head
            9,              // record
            1,              // N - bytes per sector size factor
            10,             // EOT (end of track)
            0,              // GPL
            0,              // DTL
        },
        {
            FDC_ST0_HD | 3, // drive number, physical head 1, no error
            0,              // no error
            0,              // no error
            8,              // cylinder
            1,              // head
            1,              // record
            1,              // N
        },
    },
    /* * * * * * */
    {
        // MT (multi-track), end record < EOT, physical head 0
        FDC_CMD_ARGS_MT_bm,
        true, // Terminal count required
        {
            3,  // Drive number
            7,  // cylinder
            0,  // head
            5,  // record
            1,  // N - bytes per sector size factor
            10, // EOT (end of track)
            0,  // GPL
            0,  // DTL
        },
        {
            3, // drive number, physical head 0, no error
            0, // no error
            0, // no error
            7, // cylinder
            0, // head
            7, // record
            1, // N
        },
    },
    {
        // MT (multi-track), end record = EOT, physical head 0
        FDC_CMD_ARGS_MT_bm,
        true, // Terminal count required
        {
            2,  // Drive number
            7,  // cylinder
            1,  // head - different from physical head just for fun
            9,  // record
            1,  // N - bytes per sector size factor
            10, // EOT (end of track)
            0,  // GPL
            0,  // DTL
        },
        {
            FDC_ST0_HD | 2, // drive number, physical head 1, no error
            0,              // no error
            0,              // no error
            7,              // cylinder
            0,              // head
            1,              // record
            1,              // N
        },
    },
    {
        // MT (multi-track), end record < EOT, physical head 1
        FDC_CMD_ARGS_MT_bm,
        true, // Terminal count required
        {
            FDC_ST0_HD | 1, // Drive number, physical head 1
            7,              // cylinder
            0,              // head
            5,              // record
            1,              // N - bytes per sector size factor
            10,             // EOT (end of track)
            0,              // GPL
            0,              // DTL
        },
        {
            FDC_ST0_HD | 1, // drive number, physical head 1, no error
            0,              // no error
            0,              // no error
            7,              // cylinder
            0,              // head
            7,              // record
            1,              // N
        },
    },
    {
        // MT (multi-track), end record = EOT, physical head 1
        FDC_CMD_ARGS_MT_bm,
        false, // Terminal count not required
        {
            FDC_ST0_HD | 0, // Drive number, physical head 1
            7,              // cylinder
            0,              // head - different from physical head just for fun
            9,              // record
            1,              // N - bytes per sector size factor
            10,             // EOT (end of track)
            0,              // GPL
            0,              // DTL
        },
        {
            0, // drive number, physical head 0, no error
            0, // no error
            0, // no error
            8, // cylinder
            1, // head
            1, // record
            1, // N
        },
    },
};

ParameterizedTestParameters(ceda_fdc, readCommand0) {
    size_t nb_params = sizeof(rwparams) / sizeof(struct rw_test_params_t);
    return cr_make_param_array(struct rw_test_params_t, rwparams, nb_params);
}

ParameterizedTest(struct rw_test_params_t *param, ceda_fdc, readCommand0) {
    uint8_t result[sizeof(param->result)];

    fdc_init();

    // Link a fake reading function
    fdc_kickDiskImage(fake_read, NULL);

    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_READ_DATA | param->cmd_alteration);

    // Send arguments checking for no error
    sendBuffer(param->arguments, sizeof(param->arguments));

    // FDC is in execution mode
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_EXM | FDC_ST_CB);

    // FDC is ready to serve data
    cr_assert_eq(fdc_getIntStatus(), true);

    // Read two full sectors
    fdc_in(FDC_ADDR_DATA_REGISTER);
    fdc_in(FDC_ADDR_DATA_REGISTER);
    fdc_in(FDC_ADDR_DATA_REGISTER);
    fdc_in(FDC_ADDR_DATA_REGISTER);

    fdc_in(FDC_ADDR_DATA_REGISTER);
    fdc_in(FDC_ADDR_DATA_REGISTER);
    fdc_in(FDC_ADDR_DATA_REGISTER);
    fdc_in(FDC_ADDR_DATA_REGISTER);

    // Stop the reading, if needed
    if (param->tc_required) {
        // FDC is still in execution mode
        assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_EXM | FDC_ST_CB);
        // Request execution termination
        fdc_tc_out(0, 0);
    }

    receiveBuffer(result, sizeof(result));

    cr_assert_arr_eq(result, param->result, sizeof(result));

    // Execution is finished
    assert_fdc_sr(FDC_ST_RQM);
}

static int fake_write(uint8_t *buffer, uint8_t unit_number, bool phy_head,
                      uint8_t phy_track, bool head, uint8_t track,
                      uint8_t sector) {
    (void)buffer;
    (void)unit_number;
    (void)phy_head;
    (void)phy_track;
    (void)head;
    (void)track;
    (void)sector;

    // In this case we force sector size to 4
    return 4;
}

ParameterizedTestParameters(ceda_fdc, writeCommand0) {
    size_t nb_params = sizeof(rwparams) / sizeof(struct rw_test_params_t);
    return cr_make_param_array(struct rw_test_params_t, rwparams, nb_params);
}

ParameterizedTest(struct rw_test_params_t *param, ceda_fdc, writeCommand0) {
    uint8_t result[sizeof(param->result)];

    fdc_init();

    // Link a fake reading function
    fdc_kickDiskImage(NULL, fake_write);

    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_WRITE_DATA | param->cmd_alteration);

    // Send arguments checking for no error
    sendBuffer(param->arguments, sizeof(param->arguments));

    // FDC is in execution mode
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_EXM | FDC_ST_CB);

    // FDC is ready to receive data
    cr_assert_eq(fdc_getIntStatus(), true);

    // Read two full sectors
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);

    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);

    // Stop the reading, if needed
    if (param->tc_required) {
        // FDC is still in execution mode
        assert_fdc_sr(FDC_ST_RQM | FDC_ST_EXM | FDC_ST_CB);
        // Request execution termination
        fdc_tc_out(0, 0);
    }

    receiveBuffer(result, sizeof(result));

    cr_assert_arr_eq(result, param->result, sizeof(result));

    // Execution is finished
    assert_fdc_sr(FDC_ST_RQM);
}

Test(ceda_fdc, formatCommand) {
    uint8_t result[7];
    uint8_t arguments[] = {
        0x01 | FDC_ST0_HD,
        0x01,
        0x02, // two sectors per track
        0x00, // gap (we don't care)
        0x35, // fill byte
    };

    fdc_init();

    // Link a fake reading function
    fdc_kickDiskImage(NULL, fake_write);

    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_FORMAT_TRACK);

    // Send arguments checking for no error
    sendBuffer(arguments, sizeof(arguments));

    // FDC is in execution mode
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_EXM | FDC_ST_CB);

    // FDC is ready to receive data
    cr_assert_eq(fdc_getIntStatus(), true);

    // First sector ID
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x01);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x01);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x01);

    fdc_out(FDC_ADDR_DATA_REGISTER, 0x00);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x01);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x02);
    fdc_out(FDC_ADDR_DATA_REGISTER, 0x01);

    // FDC is still in execution mode
    // TODO(giuliof): This is not always true
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_EXM | FDC_ST_CB);

    // Stop the writing
    fdc_tc_out(0, 0);

    receiveBuffer(result, sizeof(result));

    // Execution is finished
    assert_fdc_sr(FDC_ST_RQM);
}
