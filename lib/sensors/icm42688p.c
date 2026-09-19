#include "imu_chip.h"

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

/* matek_h743's real onboard IMU -- SPI1 via board_imu_spi_*() (see
   board.h's own comment for pin/bus provenance, issue #26). Implements
   imu_chip.h's contract; imu.c owns the task/queue plumbing that calls
   this.

   Register map, WHO_AM_I value, FSR/ODR config, and scale factors are
   ported from aoa-boat-controller's own real ImuReader
   (lib/Imu/H743/ImuReader), itself taken from Betaflight's real, deployed
   drivers/accgyro/accgyro_spi_icm426xx.c -- independently cross-checked
   here against that same file's current upstream source AND Rotorflight's
   own accgyro_spi_icm426xx.c fork (both agree exactly on every register
   address, the WHO_AM_I constant, and the ODR/FSR encoding used below).
   Not re-derived from the datasheet from scratch.

   Deliberately a smaller init sequence than either reference driver, not
   an oversight -- same scope cut aoa-boat-controller's own ImuReader.h
   documents: no Anti-Alias Filter config (factory default, 258Hz-1962Hz
   range, is nowhere near this project's actual polling rate), no
   interrupt/EXTI pin (this driver polls once per imu.c task tick), and no
   offset/bias calibration (imu_chip.h's contract is raw reads only --
   ImuSample has no offset fields yet, see imu.h). */

/* Register addresses -- all User Bank 0 (the power-on-reset default
   bank); nothing here touches REG_BANK_SEL (0x76) at all. */
#define REG_DEVICE_CONFIG 0x11 /* soft reset */
#define REG_PWR_MGMT0 0x4E
#define REG_GYRO_CONFIG0 0x4F
#define REG_ACCEL_CONFIG0 0x50
#define REG_WHO_AM_I 0x75

/* TEMP_DATA1 (0x1D), ACCEL_DATA_X1 (0x1F), GYRO_DATA_X1 (0x25) are
   contiguous (0x1D-0x2A: 2-byte temp, then two 6-byte X/Y/Z burst blocks,
   each high-byte-first) -- one burst read starting here instead of three
   separate transactions, same as both reference drivers. */
#define REG_TEMP_DATA1 0x1D
#define BURST_LEN 14 /* 2 (temp) + 6 (accel) + 6 (gyro) */

#define DEVICE_CONFIG_SOFT_RESET_BIT (1 << 0)

/* PWR_MGMT0: bits[1:0] accel mode, bits[3:2] gyro mode (11 = Low Noise). */
#define PWR_MGMT0_ACCEL_MODE_LN (0x03 << 0)
#define PWR_MGMT0_GYRO_MODE_LN (0x03 << 2)
#define PWR_MGMT0_IDLE 0x00
#define PWR_MGMT0_LN_BOTH (PWR_MGMT0_ACCEL_MODE_LN | PWR_MGMT0_GYRO_MODE_LN)

/* GYRO_CONFIG0/ACCEL_CONFIG0: bits[7:5] full-scale select, bits[3:0] ODR.
   FS_SEL=0 on the ICM42688P is the chip's max range: +-2000dps/+-16g
   (confirmed against both reference drivers' FS_SEL tables -- other
   chips in the same family need FS_SEL=1 for this range, the 42688P
   doesn't). ODR value 6 = 1kHz (both references' odrLUT[ODR_CONFIG_1K]),
   comfortably faster than imu.c's own 20ms/50Hz polling rate. */
#define FULL_SCALE_MAX (0x00 << 5)
#define ODR_1KHZ 0x06
#define CONFIG_MAX_RANGE_1KHZ (FULL_SCALE_MAX | ODR_1KHZ)

#define WHO_AM_I_ICM42688P 0x47

/* +-2000dps range: 2000.0/32768 dps per LSB. */
#define GYRO_DPS_PER_LSB (2000.0f / 32768.0f)
/* +-16g range: 2048 LSB/g. */
#define ACCEL_G_PER_LSB (1.0f / 2048.0f)

static int16_t combine_big_endian(uint8_t high, uint8_t low) {
    return (int16_t)(((uint16_t)high << 8) | low);
}

bool imu_chip_init(void) {
    board_imu_spi_init();

    /* Soft reset, then idle (accel+gyro off) before configuring -- same
       order both reference drivers use. */
    board_imu_spi_write_reg(REG_DEVICE_CONFIG, DEVICE_CONFIG_SOFT_RESET_BIT);
    vTaskDelay(pdMS_TO_TICKS(1)); /* power-on/reset settling time */
    board_imu_spi_write_reg(REG_PWR_MGMT0, PWR_MGMT0_IDLE);

    /* Poll WHO_AM_I -- up to 20 attempts, 1ms apart, matching both
       reference drivers' retry loop. */
    bool detected = false;
    for (uint8_t attempt = 0; attempt < 20; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(1));
        uint8_t whoAmI = 0;
        board_imu_spi_read_regs(REG_WHO_AM_I, &whoAmI, 1);
        if (whoAmI == WHO_AM_I_ICM42688P) {
            detected = true;
            break;
        }
    }
    if (!detected) {
        return false;
    }

    /* Turn accel+gyro on in Low Noise mode before setting full-scale/ODR
       -- the ICM42688P datasheet requires ODR/FSR writes to happen with
       the sensors already powered, per both reference drivers' own
       "turn gyro/acc on again so ODR and FSR can be configured" comment. */
    board_imu_spi_write_reg(REG_PWR_MGMT0, PWR_MGMT0_LN_BOTH);
    vTaskDelay(pdMS_TO_TICKS(1));

    board_imu_spi_write_reg(REG_GYRO_CONFIG0, CONFIG_MAX_RANGE_1KHZ);
    vTaskDelay(pdMS_TO_TICKS(15)); /* post-ODR-write settling delay */
    board_imu_spi_write_reg(REG_ACCEL_CONFIG0, CONFIG_MAX_RANGE_1KHZ);
    vTaskDelay(pdMS_TO_TICKS(15));

    return true;
}

bool imu_chip_read(float accel_g[3], float gyro_dps[3]) {
    uint8_t burst[BURST_LEN];
    board_imu_spi_read_regs(REG_TEMP_DATA1, burst, BURST_LEN);

    for (uint8_t axis = 0; axis < 3; ++axis) {
        int16_t const rawAccel = combine_big_endian(burst[2 + axis * 2], burst[3 + axis * 2]);
        int16_t const rawGyro = combine_big_endian(burst[8 + axis * 2], burst[9 + axis * 2]);
        accel_g[axis] = (float)rawAccel * ACCEL_G_PER_LSB;
        gyro_dps[axis] = (float)rawGyro * GYRO_DPS_PER_LSB;
    }

    /* Plain SPI reads over a bus with no other device sharing these
       lines -- nothing here can fail short of the chip being physically
       gone, which imu_chip_init()'s WHO_AM_I check already gates. */
    return true;
}
