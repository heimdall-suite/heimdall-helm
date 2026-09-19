#include "baro_chip.h"

/* Placeholder baro_chip.h implementation for any board without a real
   chip driver yet -- nexus_xr today (HELM_HAS_BARO is 0 there, no
   confirmed hardware, despite .docs/hardware.md noting an SPL06-001 is
   likely present). Same role as imu_stub.c. This is the default
   custom_helm_baro selection (see scripts/add_sensors.py) -- boards get
   this unless their env names a real chip file. */

bool baro_chip_init(void) {
    return false;
}

bool baro_chip_read(float *pressure_pa, float *temperature_c) {
    (void)pressure_pa;
    (void)temperature_c;
    return false;
}
