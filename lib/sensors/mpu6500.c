#include "imu_chip.h"

#include "board.h"

#include "FreeRTOS.h"
#include "task.h"

/* afroflight32's real onboard IMU -- I2C2 via board_i2c2_*() (see
   board.h's own comment for pin/bus provenance, issue #27). Implements
   imu_chip.h's contract; imu.c owns the task/queue plumbing that calls
   this.

   Register map, WHO_AM_I value, and init sequence are ported from
   Betaflight's real, deployed drivers/accgyro/accgyro_mpu6500.c and its
   shared drivers/accgyro/accgyro_mpu.c/accgyro_mpu.h (register
   addresses, reset/clock-select/config write order, and the
   mpuGyroDLPF() "normal" default this driver's DLPF_CFG choice matches)
   -- not re-derived from the datasheet from scratch, and not from
   aoa-boat-controller's own MPU6500_WE-library-based ImuReader (that
   library's internal register writes aren't directly inspectable/
   redistributable the way Betaflight's real source is, and this
   project's own convention is raw HAL/LL, no third-party Arduino
   libraries).

   Full-scale ranges (+-2000dps / +-16g) match this project's own
   icm42688p.c (matek_h743's IMU) for consistency between the two
   boards' scaffolding, not because the two chips need to match. DLPF_CFG
   is left at Betaflight's own current default (0 -- "normal" hardware
   LPF mode, ~250Hz gyro bandwidth, 8kHz internal rate) rather than
   aoa-boat-controller's own hand-tuned ~41Hz/~5Hz split -- that tuning
   was picked against a specific control law this project hasn't
   designed yet (see .docs/architecture/control-loops.md's status), so
   adopting it here would bake in an assumption imu_chip.h's raw-reads-
   only contract doesn't ask for. SMPLRT_DIV=7 divides that 8kHz internal
   rate down to 1kHz, matching icm42688p.c's own ODR choice. */

#define MPU_RA_SMPLRT_DIV 0x19
#define MPU_RA_CONFIG 0x1A /* DLPF_CFG, bits[2:0] */
#define MPU_RA_GYRO_CONFIG 0x1B
#define MPU_RA_ACCEL_CONFIG 0x1C
#define MPU_RA_SIGNAL_PATH_RESET 0x68
#define MPU_RA_PWR_MGMT_1 0x6B
#define MPU_RA_WHO_AM_I 0x75
#define MPU_RA_ACCEL_XOUT_H 0x3B

/* ACCEL_XOUT_H (0x3B) through GYRO_ZOUT_L (0x48) is one contiguous
   14-byte burst: 6 bytes accel, 2 bytes temp (unused -- imu_chip.h's
   contract has no temperature field), 6 bytes gyro, all high-byte-first. */
#define BURST_LEN 14

#define MPU6500_ADDR 0x68 /* 7-bit I2C address, confirmed against both
                              cleanflight's target.h and this project's
                              own board_features.h HELM_HAS_IMU comment */
#define MPU6500_WHO_AM_I 0x70 /* genuinely MPU6500, not MPU9250's 0x71 --
                                  see board_features.h's own comment on
                                  which chip this exact board carries */

#define PWR_MGMT_1_RESET (1 << 7)
#define PWR_MGMT_1_CLK_PLL 0x01 /* CLKSEL=1: PLL with X-axis gyro reference */

/* GYRO_CONFIG/ACCEL_CONFIG: bits[4:3] full-scale select. INV_FSR_2000DPS
   and INV_FSR_16G are both index 3 in Betaflight's own gyro_fsr_e/
   accel_fsr_e enums (0=lowest range), i.e. these chips' max range. */
#define GYRO_FS_SEL_2000DPS (0x03 << 3)
#define ACCEL_FS_SEL_16G (0x03 << 3)

#define DLPF_CFG_NORMAL 0x00
#define SMPLRT_DIV_1KHZ 0x07 /* 8kHz / (1 + 7) = 1kHz */

/* +-2000dps range: 2000.0/32768 dps per LSB -- same convention (and same
   physical result) as icm42688p.c's own GYRO_DPS_PER_LSB. */
#define GYRO_DPS_PER_LSB (2000.0f / 32768.0f)
/* +-16g range: 2048 LSB/g (Betaflight's own acc_1G = 512*4 for this
   exact range/chip). */
#define ACCEL_G_PER_LSB (1.0f / 2048.0f)

static int16_t combine_big_endian(uint8_t high, uint8_t low) {
    return (int16_t)(((uint16_t)high << 8) | low);
}

bool imu_chip_init(void) {
    board_i2c2_init();

    uint8_t whoAmI = 0;
    if (!board_i2c2_read_regs(MPU6500_ADDR, MPU_RA_WHO_AM_I, &whoAmI, 1) || whoAmI != MPU6500_WHO_AM_I) {
        return false;
    }

    /* Reset, then reset signal paths, then wake -- same order Betaflight's
       real mpu6500GyroInit() uses, including its 100ms settling delays
       around each reset step. */
    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_PWR_MGMT_1, PWR_MGMT_1_RESET)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_SIGNAL_PATH_RESET, 0x07)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_PWR_MGMT_1, 0x00)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_PWR_MGMT_1, PWR_MGMT_1_CLK_PLL)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(15));

    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_GYRO_CONFIG, GYRO_FS_SEL_2000DPS)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(15));
    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_ACCEL_CONFIG, ACCEL_FS_SEL_16G)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(15));
    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_CONFIG, DLPF_CFG_NORMAL)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(15));
    if (!board_i2c2_write_reg(MPU6500_ADDR, MPU_RA_SMPLRT_DIV, SMPLRT_DIV_1KHZ)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    return true;
}

bool imu_chip_read(float accel_g[3], float gyro_dps[3]) {
    uint8_t burst[BURST_LEN];
    if (!board_i2c2_read_regs(MPU6500_ADDR, MPU_RA_ACCEL_XOUT_H, burst, BURST_LEN)) {
        return false;
    }

    for (uint8_t axis = 0; axis < 3; ++axis) {
        int16_t const rawAccel = combine_big_endian(burst[axis * 2], burst[axis * 2 + 1]);
        /* Skip the 2 temp bytes at burst[6:7] -- gyro starts at burst[8]. */
        int16_t const rawGyro = combine_big_endian(burst[8 + axis * 2], burst[9 + axis * 2]);
        accel_g[axis] = (float)rawAccel * ACCEL_G_PER_LSB;
        gyro_dps[axis] = (float)rawGyro * GYRO_DPS_PER_LSB;
    }

    return true;
}
