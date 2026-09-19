#include "baro_chip.h"

#include "board.h"

/* afroflight32's real onboard baro -- I2C2 via board_i2c2_*() (see
   board.h's own comment for pin/bus provenance, issue #23), the SAME
   bus this board's MPU6500 IMU already brings up (board_i2c2_init() is
   idempotent, see board.c) -- different address (0x76 vs the IMU's
   0x68), no conflict.

   Register map, chip ID (0x58), and the double-precision compensation
   formulas are ported from aoa-boat-controller's own real BaroReader
   (lib/Baro/Naze32/BaroReader), itself a field-for-field port of Bosch's
   own official reference driver's bmp280_compensate_T_double/
   bmp280_compensate_P_double. Bosch's own driver repo is no longer
   reachable to re-diff directly, so this was independently cross-checked
   two other ways instead: (1) a public mirror of that same original
   Bosch double-precision implementation (matches this file's formulas
   term-for-term), and (2) Betaflight's real, deployed
   drivers/barometer/barometer_bmp280.c and Adafruit's widely-used
   Adafruit_BMP280_Library, both of which implement Bosch's alternate
   int32/int64 fixed-point variant of the exact same algorithm (same
   register map, same calibration layout, mathematically equivalent
   compensation) -- not a from-scratch derivation either way. */

#define BMP280_ADDR 0x76
#define CHIP_ID_BMP280 0x58

#define REG_CALIB_START 0x88 /* 24 bytes, dig_T1..dig_P9 */
#define REG_CHIP_ID 0xD0
#define REG_CONFIG 0xF5
#define REG_CTRL_MEAS 0xF4
#define REG_PRESS_MSB 0xF7 /* 6-byte burst: press(3)+temp(3) */

/* ctrl_meas encodings (Bosch BMP280 datasheet section 4.3.4/4.3.5). */
#define CTRL_MEAS_SLEEP 0x00 /* mode=00 (sleep) -- config register writes only take effect here */
/* osrs_t=x2 (010), osrs_p=x16 (101), mode=normal (11) -- matches
   aoa-boat-controller's own "jitter-free over fast" choice: x16 pressure
   oversampling is the datasheet's own highest-resolution setting, x2
   temperature oversampling is already far below this chip's noise floor
   at any higher setting. */
#define CTRL_MEAS_NORMAL_HIGH_RES 0x57
/* t_sb=1000ms (101), filter=16 (100, the strongest IIR coefficient this
   chip has), spi3w_en=0 (I2C). A slow 1s standby matches this project's
   own baro.c task period -- this is a reference value sampled roughly
   once a second, not a control input. */
#define CONFIG_FILTER16_STANDBY1S 0xB0

static uint16_t digT1;
static int16_t digT2, digT3;
static uint16_t digP1;
static int16_t digP2, digP3, digP4, digP5, digP6, digP7, digP8, digP9;
static int32_t tFine;

/* 24 bytes starting at 0x88, little-endian pairs in this exact order
   (Bosch BMP280 datasheet Table 21 "Register map trimming parameters"):
   dig_T1 (u16), dig_T2 (s16), dig_T3 (s16), dig_P1 (u16), dig_P2..dig_P9
   (s16 each). */
static bool read_calibration(void) {
    uint8_t raw[24];
    if (!board_i2c2_read_regs(BMP280_ADDR, REG_CALIB_START, raw, sizeof(raw))) {
        return false;
    }

    digT1 = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
    digT2 = (int16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8));
    digT3 = (int16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
    digP1 = (uint16_t)raw[6] | ((uint16_t)raw[7] << 8);
    digP2 = (int16_t)((uint16_t)raw[8] | ((uint16_t)raw[9] << 8));
    digP3 = (int16_t)((uint16_t)raw[10] | ((uint16_t)raw[11] << 8));
    digP4 = (int16_t)((uint16_t)raw[12] | ((uint16_t)raw[13] << 8));
    digP5 = (int16_t)((uint16_t)raw[14] | ((uint16_t)raw[15] << 8));
    digP6 = (int16_t)((uint16_t)raw[16] | ((uint16_t)raw[17] << 8));
    digP7 = (int16_t)((uint16_t)raw[18] | ((uint16_t)raw[19] << 8));
    digP8 = (int16_t)((uint16_t)raw[20] | ((uint16_t)raw[21] << 8));
    digP9 = (int16_t)((uint16_t)raw[22] | ((uint16_t)raw[23] << 8));

    return true;
}

bool baro_chip_init(void) {
    board_i2c2_init();

    uint8_t chipId = 0;
    if (!board_i2c2_read_regs(BMP280_ADDR, REG_CHIP_ID, &chipId, 1) || chipId != CHIP_ID_BMP280) {
        return false;
    }

    if (!read_calibration()) {
        return false;
    }

    /* config register writes are only guaranteed to take effect in sleep
       mode (Bosch datasheet 3.3.4) -- go to sleep first, write config,
       THEN switch to normal mode to actually start continuous
       conversions. */
    if (!board_i2c2_write_reg(BMP280_ADDR, REG_CTRL_MEAS, CTRL_MEAS_SLEEP)) {
        return false;
    }
    if (!board_i2c2_write_reg(BMP280_ADDR, REG_CONFIG, CONFIG_FILTER16_STANDBY1S)) {
        return false;
    }
    if (!board_i2c2_write_reg(BMP280_ADDR, REG_CTRL_MEAS, CTRL_MEAS_NORMAL_HIGH_RES)) {
        return false;
    }

    return true;
}

/* Bosch reference driver's double-precision compensation
   (bmp280_compensate_T_double), ported field-for-field -- see this
   file's own header comment for the sources this was cross-checked
   against. Sets tFine, which compensate_pressure() below depends on --
   callers must always compensate temperature before pressure for a
   given sample, same dependency the reference driver itself has. */
static float compensate_temperature(int32_t adcT) {
    double const var1 = ((double)adcT / 16384.0 - (double)digT1 / 1024.0) * (double)digT2;
    double const var2 = (((double)adcT / 131072.0 - (double)digT1 / 8192.0) *
                          ((double)adcT / 131072.0 - (double)digT1 / 8192.0)) *
                         (double)digT3;
    tFine = (int32_t)(var1 + var2);
    return (float)((var1 + var2) / 5120.0);
}

/* Bosch reference driver's double-precision compensation
   (bmp280_compensate_P_double), ported field-for-field, including the
   div-by-zero guard the reference driver itself has. */
static float compensate_pressure(int32_t adcP) {
    double var1 = (double)tFine / 2.0 - 64000.0;
    double var2 = var1 * var1 * (double)digP6 / 32768.0;
    var2 = var2 + var1 * (double)digP5 * 2.0;
    var2 = (var2 / 4.0) + ((double)digP4 * 65536.0);
    var1 = ((double)digP3 * var1 * var1 / 524288.0 + (double)digP2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)digP1;
    if (var1 == 0.0) {
        return 0.0f;
    }
    double p = 1048576.0 - (double)adcP;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = (double)digP9 * p * p / 2147483648.0;
    var2 = p * (double)digP8 / 32768.0;
    p = p + (var1 + var2 + (double)digP7) / 16.0;
    return (float)p;
}

bool baro_chip_read(float *pressure_pa, float *temperature_c) {
    uint8_t raw[6];
    if (!board_i2c2_read_regs(BMP280_ADDR, REG_PRESS_MSB, raw, sizeof(raw))) {
        return false;
    }

    int32_t const adcP = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t const adcT = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);

    /* Temperature MUST be compensated first -- it sets tFine, which
       compensate_pressure() reads. */
    *temperature_c = compensate_temperature(adcT);
    *pressure_pa = compensate_pressure(adcP);

    return true;
}
