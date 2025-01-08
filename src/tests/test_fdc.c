
#include <criterion/criterion.h>

// TODO source path is src!
#include "../fdc.h"
#include "../fdc_registers.h"

Test(ceda_fdc, mainStatusRegisterWhenIdle) {
    fdc_init();

    // Try to read status register and check that it is idle
    uint8_t sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7));
}

Test(ceda_fdc, specifyCommand) {
    fdc_init();

    uint8_t sr;

    // Try to read status register and check that it is idle
    fdc_out(ADDR_DATA_REGISTER, SPECIFY);

    // Now read status register to check that FDC is ready to receive arguments
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7) | (1 << 4));

    // Pass dummy arguments
    fdc_out(ADDR_DATA_REGISTER, 0x00);
    fdc_out(ADDR_DATA_REGISTER, 0x00);

    // FDC is no more busy
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7));
}

Test(ceda_fdc, senseInterruptStatusCommand) {
    fdc_init();

    uint8_t sr;

    fdc_out(ADDR_DATA_REGISTER, SENSE_INTERRUPT);

    // This command has no arguments
    // FDC should be ready to give response
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7) | (1 << 6) | (1 << 4));

    // First response byte is SR0 with interrupt code = 0 and Seek End = 1
    sr = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(sr, (1 << 5));

    // FDC has another byte of response
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7) | (1 << 6) | (1 << 4));

    // Second response byte is current cylinder, which should be zero at reset
    sr = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(sr, 0);
}

Test(ceda_fdc, seekCommand) {
    fdc_init();

    uint8_t sr;

    fdc_out(ADDR_DATA_REGISTER, SEEK);

    // Now read status register to check that FDC is ready to receive arguments
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7) | (1 << 4));

    // First argument is number of drive
    fdc_out(ADDR_DATA_REGISTER, 0x00);
    // Second argument is cylinder position
    fdc_out(ADDR_DATA_REGISTER, 5);

    // FDC is no more busy
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7));

    // A sense interrupt command is expected after SEEK
    fdc_out(ADDR_DATA_REGISTER, SENSE_INTERRUPT);

    // This command has no arguments
    // FDC should be ready to give response
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7) | (1 << 6) | (1 << 4));

    // First response byte is SR0 with interrupt code = 0 and Seek End = 1
    sr = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(sr, (1 << 5));

    // FDC has another byte of response
    sr = fdc_in(ADDR_STATUS_REGISTER);
    cr_expect_eq(sr, (1 << 7) | (1 << 6) | (1 << 4));

    // Second response byte is current cylinder, which should be the one
    // specified by the seek argument
    sr = fdc_in(ADDR_DATA_REGISTER);
    cr_expect_eq(sr, 5);
}
