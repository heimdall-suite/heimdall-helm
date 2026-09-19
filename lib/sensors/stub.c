#include "imu_chip.h"

/* Placeholder imu_chip.h implementation for any board without a real
   chip driver yet -- afroflight32 until issue #27 lands its mpu6500.c.
   Preserves #14's original behavior exactly (imu_get_latest() always
   reports SENSOR_STATUS_FAILED) now that imu.c calls into a real chip
   interface instead of hardcoding that status itself. This is the
   default custom_helm_imu selection (see scripts/add_sensors.py) --
   boards get this unless their env names a real chip file. */

bool imu_chip_init(void) {
    return false;
}

bool imu_chip_read(float accel_g[3], float gyro_dps[3]) {
    (void)accel_g;
    (void)gyro_dps;
    return false;
}
