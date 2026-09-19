#ifndef HELM_BARO_H
#define HELM_BARO_H

#include "sensors.h"

/* Second concrete sensor built on sensors.h's shared status, same shape
   as imu.h (issue #14's scaffolding, imu.c's own task/queue/supervisor
   plumbing pattern) -- HELM_HAS_BARO is a compile-time hardware fact in
   board_features.h, not a deliberate HELM_FEATURE_* toggle, same
   reasoning as imu.h's own header comment. baro_start() and this whole
   module are gated on that flag directly.

   Real chip decode is a per-board driver file behind baro_chip.h's
   internal contract, explicit-selected at build time (issue #23:
   dps310.c/matek_h743, bmp280.c/afroflight32 -- see lib/README.md's
   "more than one chip in a subsystem" section, same template imu.c's
   icm42688p.c/mpu6500.c split already uses) -- baro.c itself stays
   chip-agnostic. A board whose chip init failed, or one still on the
   baro_stub.c placeholder, always reports SENSOR_STATUS_FAILED here,
   never a stale/zeroed sample dressed up as OK.

   Soft-dependency from the start, per .docs/architecture/sensors.md's
   "Hard-required vs. optional/soft-dependency" section -- nothing
   currently consumes pressure data as hard-required; whether/how a
   future consumer (altitude hold?) uses it is a separate, later
   decision, not this module's concern. */
typedef struct {
    float pressure_pa;    /* station pressure, Pa -- only meaningful when status is OK */
    float temperature_c;  /* chip's own die temperature, deg C -- only meaningful when status is OK */
    SensorStatus status;
} BaroSample;

void baro_start(void);
void baro_get_latest(BaroSample *out);

#endif /* HELM_BARO_H */
