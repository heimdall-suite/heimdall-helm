#include "baro_chip.h"

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

/* matek_h743's real onboard baro -- I2C2 via board_i2c2_*() (see
   board.h's own comment for pin/bus provenance, issue #23). Implements
   baro_chip.h's contract; baro.c owns the task/queue plumbing that calls
   this.

   Register map, chip ID, calibration coefficient bit layout, and
   compensation formulas are ported from aoa-boat-controller's own real
   BaroReader (lib/Baro/H743/BaroReader), itself a field-for-field port
   of Betaflight's real, deployed
   drivers/barometer/barometer_dps310.c -- independently cross-checked
   here against that same file's current upstream source, which agrees
   exactly on every register address, the chip ID (0x10), the 32Hz/16x-
   oversampling config sequence, and the 253952 scale factor used below.
   Not re-derived from the datasheet from scratch.

   This exact physical unit's chip identity is flagged in
   board_features.h as "confirmed present in code, verify wiring" (one
   notch weaker than the IMU's plain "confirmed") -- Betaflight/ArduPilot
   both probe this board for either a DPS310 or a BMP280 at the same
   address (chip identity varies by production run), so a chip-ID
   mismatch here on real hardware means "this unit has the other chip,"
   not necessarily a wiring bug -- see baro_chip_init()'s own return. */

#define DPS310_ADDR 0x76

#define REG_PSR_B2 0x00 /* 6-byte burst: PSR_B2,B1,B0,TMP_B2,B1,B0 */
#define REG_PRS_CFG 0x06
#define REG_TMP_CFG 0x07
#define REG_MEAS_CFG 0x08
#define REG_CFG_REG 0x09
#define REG_RESET 0x0C
#define REG_ID 0x0D
#define REG_COEF 0x10 /* 18 bytes, registers 0x10-0x21 */
#define REG_COEF_SRCE 0x28
#define COEF_COUNT 18

#define CHIP_ID_DPS310 0x10 /* DPS310_ID_REV_AND_PROD_ID */

#define RESET_SOFT_RST 0x09 /* 0b1001, datasheet's own reset command bits */

#define MEAS_CFG_COEF_RDY (1 << 7)
#define MEAS_CFG_SENSOR_RDY (1 << 6)
#define MEAS_CFG_CONTINUOUS 0x07 /* MEAS_CTRL=111: continuous pressure+temperature */

/* PRS_CFG/TMP_CFG: 101=32 measurements/sec (PM_RATE/TMP_RATE), 0100=16x
   oversampling (PM_PRC/TMP_PRC) -- the datasheet's own "Standard"
   precision/rate preset; kScaleFactor16x below only applies at this
   exact oversampling. */
#define CFG_BIT_RATE_32HZ 0x50
#define CFG_BIT_OSR_16 0x04
#define TMP_CFG_BIT_EXT_SENSOR 0x80
#define COEF_SRCE_BIT_TMP_COEF_SRCE 0x80

/* CFG_REG: result bit-shift required whenever oversampling exceeds 8x
   (datasheet section 4.4) -- P_SHIFT and T_SHIFT both set, since both
   channels use 16x here. */
#define CFG_REG_BIT_P_SHIFT 0x04
#define CFG_REG_BIT_T_SHIFT 0x08

/* Scaling factor (datasheet Table 9, "16 times (Standard)" row) -- only
   valid at the exact 16x oversampling configured above. */
#define SCALE_FACTOR_16X 253952.0f

typedef struct {
    int32_t c0;
    int32_t c1;
    int32_t c00;
    int32_t c10;
    int32_t c01;
    int32_t c11;
    int32_t c20;
    int32_t c21;
    int32_t c30;
} Calibration;

static Calibration calib;

static int32_t sign_extend(uint32_t raw, uint8_t bitLength) {
    if (raw & ((uint32_t)1 << (bitLength - 1))) {
        return (int32_t)raw - ((int32_t)1 << bitLength);
    }
    return (int32_t)raw;
}

/* Only sets bits that aren't already set -- same read-modify-write idiom
   the reference driver's own registerSetBits() uses. Returns false if
   either the read or the write failed. */
static bool set_bits(uint8_t reg, uint8_t bitsToSet) {
    uint8_t value = 0;
    if (!board_i2c2_read_regs(DPS310_ADDR, reg, &value, 1)) {
        return false;
    }
    if ((value & bitsToSet) == bitsToSet) {
        return true;
    }
    return board_i2c2_write_reg(DPS310_ADDR, reg, (uint8_t)(value | bitsToSet));
}

/* 18 bytes starting at 0x10 -- bit layout ported field-for-field from the
   reference driver (datasheet section 8.11, Calibration Coefficients). */
static bool read_calibration(void) {
    uint8_t coef[COEF_COUNT];
    if (!board_i2c2_read_regs(DPS310_ADDR, REG_COEF, coef, COEF_COUNT)) {
        return false;
    }

    /* 0x10 c0[11:4] + 0x11 c0[3:0] */
    calib.c0 = sign_extend(((uint32_t)coef[0] << 4) | (((uint32_t)coef[1] >> 4) & 0x0F), 12);
    /* 0x11 c1[11:8] + 0x12 c1[7:0] */
    calib.c1 = sign_extend((((uint32_t)coef[1] & 0x0F) << 8) | (uint32_t)coef[2], 12);
    /* 0x13 c00[19:12] + 0x14 c00[11:4] + 0x15 c00[3:0] */
    calib.c00 = sign_extend(((uint32_t)coef[3] << 12) | ((uint32_t)coef[4] << 4) | (((uint32_t)coef[5] >> 4) & 0x0F),
                             20);
    /* 0x15 c10[19:16] + 0x16 c10[15:8] + 0x17 c10[7:0] */
    calib.c10 = sign_extend((((uint32_t)coef[5] & 0x0F) << 16) | ((uint32_t)coef[6] << 8) | (uint32_t)coef[7], 20);
    /* 0x18 c01[15:8] + 0x19 c01[7:0] */
    calib.c01 = sign_extend(((uint32_t)coef[8] << 8) | (uint32_t)coef[9], 16);
    /* 0x1A c11[15:8] + 0x1B c11[7:0] */
    calib.c11 = sign_extend(((uint32_t)coef[10] << 8) | (uint32_t)coef[11], 16);
    /* 0x1C c20[15:8] + 0x1D c20[7:0] */
    calib.c20 = sign_extend(((uint32_t)coef[12] << 8) | (uint32_t)coef[13], 16);
    /* 0x1E c21[15:8] + 0x1F c21[7:0] */
    calib.c21 = sign_extend(((uint32_t)coef[14] << 8) | (uint32_t)coef[15], 16);
    /* 0x20 c30[15:8] + 0x21 c30[7:0] */
    calib.c30 = sign_extend(((uint32_t)coef[16] << 8) | (uint32_t)coef[17], 16);

    return true;
}

bool baro_chip_init(void) {
    board_i2c2_init();

    uint8_t chipId = 0;
    if (!board_i2c2_read_regs(DPS310_ADDR, REG_ID, &chipId, 1) || chipId != CHIP_ID_DPS310) {
        return false;
    }

    if (!set_bits(REG_RESET, RESET_SOFT_RST)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(40)); /* datasheet-specified reset settle time */

    uint8_t status = 0;
    if (!board_i2c2_read_regs(DPS310_ADDR, REG_MEAS_CFG, &status, 1)) {
        return false;
    }
    if ((status & MEAS_CFG_COEF_RDY) == 0 || (status & MEAS_CFG_SENSOR_RDY) == 0) {
        return false;
    }

    if (!read_calibration()) {
        return false;
    }

    if (!set_bits(REG_PRS_CFG, CFG_BIT_RATE_32HZ | CFG_BIT_OSR_16)) {
        return false;
    }

    uint8_t coefSrce = 0;
    if (!board_i2c2_read_regs(DPS310_ADDR, REG_COEF_SRCE, &coefSrce, 1)) {
        return false;
    }
    uint8_t const tempCoefSource = coefSrce & COEF_SRCE_BIT_TMP_COEF_SRCE;
    if (!set_bits(REG_TMP_CFG, (uint8_t)(CFG_BIT_RATE_32HZ | CFG_BIT_OSR_16 | tempCoefSource))) {
        return false;
    }

    if (!set_bits(REG_CFG_REG, CFG_REG_BIT_P_SHIFT | CFG_REG_BIT_T_SHIFT)) {
        return false;
    }
    if (!set_bits(REG_MEAS_CFG, MEAS_CFG_CONTINUOUS)) {
        return false;
    }

    return true;
}

bool baro_chip_read(float *pressure_pa, float *temperature_c) {
    uint8_t raw[6];
    if (!board_i2c2_read_regs(DPS310_ADDR, REG_PSR_B2, raw, sizeof(raw))) {
        return false;
    }

    int32_t const pRaw = sign_extend(((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | (uint32_t)raw[2], 24);
    int32_t const tRaw = sign_extend(((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | (uint32_t)raw[5], 24);

    float const pRawSc = (float)pRaw / SCALE_FACTOR_16X;
    float const tRawSc = (float)tRaw / SCALE_FACTOR_16X;

    /* Compensation formulas ported field-for-field from the reference
       driver's dps310GetUP() -- see this file's own header comment for
       the source this was cross-checked against (datasheet sections
       4.9.1/4.9.2). */
    float const c00 = (float)calib.c00;
    float const c01 = (float)calib.c01;
    float const c10 = (float)calib.c10;
    float const c11 = (float)calib.c11;
    float const c20 = (float)calib.c20;
    float const c21 = (float)calib.c21;
    float const c30 = (float)calib.c30;

    *pressure_pa = c00 + pRawSc * (c10 + pRawSc * (c20 + pRawSc * c30)) + tRawSc * c01 +
                   tRawSc * pRawSc * (c11 + pRawSc * c21);

    float const c0 = (float)calib.c0;
    float const c1 = (float)calib.c1;
    *temperature_c = c0 * 0.5f + c1 * tRawSc;

    return true;
}
