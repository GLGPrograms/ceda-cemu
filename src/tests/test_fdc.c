
#include <criterion/criterion.h>

// TODO source path is src!
#include "../fdc.h"
#include "../fdc_registers.h"

static void assert_fdc_sr(uint8_t expected_sr);
static int fake_read(uint8_t *buffer, uint8_t unit_number, bool phy_head,
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

Test(ceda_fdc, readCommandNoMedium) {
    fdc_init();

    fdc_out(FDC_ADDR_DATA_REGISTER, FDC_READ_DATA);

    /* Provide the argument, dummy ones! */
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 1st argument is number of drive
    fdc_out(FDC_ADDR_DATA_REGISTER, 0);
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 2nd argument is cylinder number
    fdc_out(FDC_ADDR_DATA_REGISTER, 1);
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 3rd argument is head number
    fdc_out(FDC_ADDR_DATA_REGISTER, 0);
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 4th argument is record number
    fdc_out(FDC_ADDR_DATA_REGISTER, 1);
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 5th argument is bytes per sector factor
    fdc_out(FDC_ADDR_DATA_REGISTER, 1);
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 6th argument is EOT
    fdc_out(FDC_ADDR_DATA_REGISTER, 5);
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 7th argument is GPL
    fdc_out(FDC_ADDR_DATA_REGISTER, 0);
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_CB);
    // 8th argument is DTL
    fdc_out(FDC_ADDR_DATA_REGISTER, 0);

    // FDC switches IO mode, but...
    assert_fdc_sr(FDC_ST_RQM | FDC_ST_DIO | FDC_ST_EXM | FDC_ST_CB);
    // ... is not ready since no medium is loaded
    cr_assert_eq(fdc_getIntStatus(), false);

    // Kick medium in...
    fdc_kickDiskImage(fake_read, NULL);
    // ... now FDC is ready
    cr_assert_eq(fdc_getIntStatus(), true);
}
