#ifndef HELM_BARO_CHIP_H
#define HELM_BARO_CHIP_H

#include <stdbool.h>

/* Internal contract between baro.c's board-agnostic task/queue plumbing
   and whichever chip file a board's env explicitly selects via
   custom_helm_baro -- dps310.c (matek_h743, issue #23), bmp280.c
   (afroflight32, issue #23), or baro_stub.c (any board without a real
   driver yet). Same shape as imu_chip.h; not part of baro.h's own
   public interface. */

/* One-time chip bring-up: identity check (chip ID register), read the
   factory calibration coefficients, and configure continuous background
   sampling. Returns false if the chip didn't respond or its identity
   didn't match. Called once, from the baro task's own context, before
   its polling loop starts. */
bool baro_chip_init(void);

/* Blocking read + compensation of the current pressure (Pa) and
   temperature (deg C) sample. Returns false on a bus/communication
   failure -- baro.c reports SENSOR_STATUS_FAILED for that tick rather
   than publishing a stale sample as OK. */
bool baro_chip_read(float *pressure_pa, float *temperature_c);

#endif /* HELM_BARO_CHIP_H */
