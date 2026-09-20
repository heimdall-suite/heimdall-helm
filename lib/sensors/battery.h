#ifndef HELM_BATTERY_H
#define HELM_BATTERY_H

#include "sensors.h"

/* Battery voltage/current sense (issue #24) -- same shared-status shape
   as imu.h/baro.h, but a different split underneath: this isn't a chip
   on a bus (no baro_chip.h-style per-board driver file), just a raw
   resistor-divider/shunt on an ADC pin -- board.h's own
   board_battery_adc_*() exposes the raw ADC counts, this file applies
   the divider-scale/amps-per-volt math on top (see board_battery_adc_
   init()'s own comment in boards/matek_h743/board.h for the full
   pin/clock provenance). HELM_HAS_BATTERY_SENSE is a compile-time
   hardware fact in board_features.h (this board's PDB brings the sense
   pins out or it doesn't), not a deliberate HELM_FEATURE_* toggle, same
   reasoning as HELM_HAS_IMU/HELM_HAS_BARO. battery_start() and this
   whole module are gated on that flag directly. */
typedef struct {
    float voltage_v; /* only meaningful when status is OK */
    float current_a; /* only meaningful when status is OK */
    SensorStatus status;
} BatterySample;

void battery_start(void);
void battery_get_latest(BatterySample *out);

#endif /* HELM_BATTERY_H */
