
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
    sr = fdc_in(ADDR_STATUS_REGISTER);
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
    assert_fdc_sr(1 << 7);
}

Test(ceda_fdc, specifyCommand) {
    fdc_init();

    // Try to read status register and check that it is idle
    fdc_out(ADDR_DATA_REGISTER, SPECIFY);

    // Now read status register to check that FDC is ready to receive arguments
    assert_fdc_sr((1 << 7) | (1 << 4));

    // Pass dummy arguments
    fdc_out(ADDR_DATA_REGISTER, 0x00);
    fdc_out(ADDR_DATA_REGISTER, 0x00);

    // FDC is no more busy
    assert_fdc_sr((1 << 7));
}

Test(ceda_fdc, senseInterruptStatusCommand) {
    fdc_init();

    uint8_t data;

    fdc_out(ADDR_DATA_REGISTER, SENSE_INTERRUPT);

    // This command has no arguments
    // FDC should be ready to give response
    assert_fdc_sr((1 << 7) | (1 << 6) | (1 << 4));

    // First response byte is SR0 with interrupt code = 0 and Seek End = 1
    data = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(data, (1 << 5));

    // FDC has another byte of response
    assert_fdc_sr((1 << 7) | (1 << 6) | (1 << 4));

    // Second response byte is current cylinder, which should be zero at reset
    data = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(data, 0);
}

Test(ceda_fdc, seekCommand) {
    fdc_init();

    uint8_t data;

    fdc_out(ADDR_DATA_REGISTER, SEEK);

    // Now read status register to check that FDC is ready to receive arguments
    assert_fdc_sr((1 << 7) | (1 << 4));

    // First argument is number of drive
    fdc_out(ADDR_DATA_REGISTER, 0x00);
    // Second argument is cylinder position
    fdc_out(ADDR_DATA_REGISTER, 5);

    // FDC is no more busy
    assert_fdc_sr((1 << 7));

    // A sense interrupt command is expected after SEEK
    fdc_out(ADDR_DATA_REGISTER, SENSE_INTERRUPT);

    // This command has no arguments
    // FDC should be ready to give response
    assert_fdc_sr((1 << 7) | (1 << 6) | (1 << 4));

    // First response byte is SR0 with interrupt code = 0 and Seek End = 1
    data = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(data, (1 << 5));

    // FDC has another byte of response
    assert_fdc_sr((1 << 7) | (1 << 6) | (1 << 4));

    // Second response byte is current cylinder, which should be the one
    // specified by the seek argument
    data = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(data, 5);
}

Test(ceda_fdc, readCommandNoMedium) {
    fdc_init();

    fdc_out(ADDR_DATA_REGISTER, READ_DATA);

    /* Provide the argument, dummy ones! */
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 1st argument is number of drive
    fdc_out(ADDR_DATA_REGISTER, 0);
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 2nd argument is cylinder number
    fdc_out(ADDR_DATA_REGISTER, 1);
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 3rd argument is head number
    fdc_out(ADDR_DATA_REGISTER, 0);
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 4th argument is record number
    fdc_out(ADDR_DATA_REGISTER, 1);
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 5th argument is bytes per sector factor
    fdc_out(ADDR_DATA_REGISTER, 1);
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 6th argument is EOT
    fdc_out(ADDR_DATA_REGISTER, 5);
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 7th argument is GPL
    fdc_out(ADDR_DATA_REGISTER, 0);
    assert_fdc_sr((1 << 7) | (1 << 4));
    // 8th argument is DTL
    fdc_out(ADDR_DATA_REGISTER, 0);

    // FDC switches IO mode, but...
    assert_fdc_sr((1 << 7) | (1 << 6) | (1 << 5) | (1 << 4));
    // ... is not ready since no medium is loaded
    cr_assert_eq(fdc_getIntStatus(), false);

    // Kick medium in...
    fdc_kickDiskImage(fake_read, NULL);
    // ... now FDC is ready
    cr_assert_eq(fdc_getIntStatus(), true);
}
