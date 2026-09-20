#ifndef HELM_GPS_H
#define HELM_GPS_H

#include <stdint.h>
#include "sensors.h"

/* GPS driver (issue #40) -- NMEA 0183 over board_gps_uart_*() (board.h).
   Same shared-status shape as imu.h/baro.h, but a genuine hot-pluggable
   peripheral, not an always-present onboard sensor: the module needs
   external power the user connects on demand, so it can be absent at
   boot or connected mid-session (board.h's own comment has the full
   provenance). status stays SENSOR_STATUS_FAILED for as long as that's
   true -- never a stale/last-known fix dressed up as OK, same
   never-fabricate-a-value discipline baro.h/lib/telemetry/sport.c
   already use. HELM_HAS_GPS is a compile-time hardware fact (this
   board's UART IS routed to a GPS header, whether or not a module
   happens to be plugged in and powered right now), not a deliberate
   HELM_FEATURE_* toggle, same reasoning as HELM_HAS_IMU/HELM_HAS_BARO.
   gps_start() and this whole module are gated on that flag directly. */
typedef struct {
    float latitude_deg;  /* decimal degrees, +N/-S -- only meaningful when status is OK */
    float longitude_deg; /* decimal degrees, +E/-W -- only meaningful when status is OK */
    float altitude_m;    /* MSL altitude, meters -- only meaningful when status is OK */
    float speed_mps;     /* ground speed, m/s -- only meaningful when status is OK, and
                             only ever set from a valid RMC sentence (see gps.c) */
    uint8_t satellites;  /* satellites used in fix -- only meaningful when status is OK */
    SensorStatus status; /* driven by GGA's own fix-quality field -- RMC only contributes
                             speed_mps on top, it never sets status itself (see gps.c) */
} GpsFix;

void gps_start(void);
void gps_get_latest(GpsFix *out);

#endif /* HELM_GPS_H */
