#ifndef HELM_IMU_CHIP_H
#define HELM_IMU_CHIP_H

#include <stdbool.h>

/* Internal contract between imu.c's board-agnostic task/queue plumbing
   (issue #14) and whichever chip file a board's env explicitly selects
   via custom_helm_imu -- icm42688p.c (matek_h743, issue #26), mpu6500.c
   (afroflight32, issue #27), or stub.c (any board without a real driver
   yet). Not part of imu.h's own public interface: only imu.c and these
   chip files need to agree on it. See lib/README.md's "more than one
   chip in a subsystem needs explicit selection" section for why this
   folder can't rely on PlatformIO's default lib/ auto-scan once more
   than one of these exists. */

/* One-time chip bring-up: reset, identity check (WHO_AM_I or
   equivalent), and initial config (full-scale range, output data rate).
   Returns false if the chip didn't respond or its identity didn't
   match -- imu.c then reports SENSOR_STATUS_FAILED permanently, same
   honest failure mode the previous plumbing-only stub had for every
   board. Called once, from the IMU task's own context, before its
   polling loop starts. */
bool imu_chip_init(void);

/* Blocking read of the current accel (g) + gyro (deg/s) sample, in each
   axis's raw physical-unit convention as the chip itself reports it (no
   board/orientation correction here -- that's a control-loop concern,
   not this driver's). Returns false on a bus/communication failure --
   imu.c reports SENSOR_STATUS_FAILED for that tick rather than
   publishing a stale or zeroed sample as OK. */
bool imu_chip_read(float accel_g[3], float gyro_dps[3]);

#endif /* HELM_IMU_CHIP_H */
