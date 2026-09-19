#include "imu_chip.h"

/* Placeholder imu_chip.h implementation for any board without a real
   chip driver yet -- nexus_xr today (HELM_HAS_IMU is 0 there, no
   confirmed hardware). Both real boards now have their own chip file
   (icm42688p.c, #26; mpu6500.c, #27); this file used to be the generic
   "stub.c" both temporarily used before #27 landed. Renamed alongside
   baro_stub.c's addition (#23) so a generic "stub.c" name doesn't have
   to silently mean "whichever subsystem happens to ask for it."
   Preserves #14's original behavior exactly (imu_get_latest() always
   reports SENSOR_STATUS_FAILED). This is the default custom_helm_imu
   selection (see scripts/add_sensors.py) -- boards get this unless
   their env names a real chip file. */

bool imu_chip_init(void) {
    return false;
}

bool imu_chip_read(float accel_g[3], float gyro_dps[3]) {
    (void)accel_g;
    (void)gyro_dps;
    return false;
}
