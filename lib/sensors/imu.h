#ifndef HELM_IMU_H
#define HELM_IMU_H

#include "sensors.h"

/* First concrete sensor built on sensors.h's shared status (issue #14's
   scaffolding). Onboard on both real boards today -- HELM_HAS_IMU is a
   compile-time hardware fact in board_features.h, not a deliberate
   HELM_FEATURE_* toggle (.docs/architecture/sensors.md's "Onboard vs.
   peripheral" section: presence here is "this board type always has the
   chip wired," same category as HELM_HAS_SBUS_UART, not a software
   budget decision). imu_start() and this whole module are gated on that
   flag directly, same idiom sport.c's whole-file #if HELM_HAS_SPORT_UART
   guard already uses.

   #14 built this module's plumbing only (task lifecycle, supervisor
   registration, the queue), same "table before source" split
   telemetry.c's #16 -> #17 used. Real chip decode is a per-board driver
   file behind imu_chip.h's internal contract, explicit-selected at
   build time (issue #26: icm42688p.c/matek_h743, issue #27: mpu6500.c/
   afroflight32 -- see lib/README.md's "more than one chip in a
   subsystem" section) -- imu.c itself stays chip-agnostic. A board
   whose chip init failed, or one still on the pre-#26/#27 stub.c
   placeholder, always reports SENSOR_STATUS_FAILED here, never a
   stale/zeroed sample dressed up as OK. */
typedef struct {
    float accel_g[3];  /* X/Y/Z, g -- only meaningful when status is OK */
    float gyro_dps[3]; /* X/Y/Z, deg/s -- only meaningful when status is OK */
    SensorStatus status;
} ImuSample;

void imu_start(void);
void imu_get_latest(ImuSample *out);

#endif /* HELM_IMU_H */
