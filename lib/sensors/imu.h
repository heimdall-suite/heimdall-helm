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

   No real chip decode yet -- this issue is plumbing only (module
   lifecycle, supervisor registration, the queue), same "table before
   source" split telemetry.c's #16 -> #17 already used. Issue #15 fills
   this in with the real ICM42688P (matek_h743) / MPU6500 (afroflight32)
   reads; until then imu_get_latest() always reports SENSOR_STATUS_FAILED
   so nothing downstream can mistake this stub for a live reading. */
typedef struct {
    float accel_g[3];  /* X/Y/Z, g -- unpopulated until #15 */
    float gyro_dps[3]; /* X/Y/Z, deg/s -- unpopulated until #15 */
    SensorStatus status;
} ImuSample;

void imu_start(void);
void imu_get_latest(ImuSample *out);

#endif /* HELM_IMU_H */
