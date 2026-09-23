#ifndef HELM_GPS_H
#define HELM_GPS_H

#include <stdint.h>
#include "sensors.h"

/* GPS driver (issue #40, refactored onto the port/protocol/source model
   by issue #56 -- .docs/architecture/ports.md) -- NMEA 0183 over
   board_gps_uart_*() (board.h). Same shared-status shape as imu.h/
   baro.h, but a genuine hot-pluggable peripheral, not an always-present
   onboard sensor: the module needs external power the user connects on
   demand, so it can be absent at boot or connected mid-session (board.h's
   own provenance comment). status stays SENSOR_STATUS_FAILED for as
   long as that's true -- never a stale/last-known fix dressed up as OK,
   same never-fabricate-a-value discipline baro.h/lib/telemetry/sport.c
   already use.

   This whole module now compiles in whenever
   HELM_HAS_GPS_UART_TRANSPORT is set (board_features.h) -- a board that
   can bind GPS to *some* port at all, not the old HELM_HAS_GPS's wrong
   conflation of wiring + role + "only one possible port" (see ports.md
   for the full history). Which port, if any, is actually claimed is
   gps.port/gps.protocol/gps.source (#54), resolved once at gps_start()
   time -- an unassigned port and an unplugged module collapse into the
   exact same FAILED case (#56's own scope note), so this file no longer
   needs to distinguish "not configured" from "configured but nothing
   answering". */
#define GPS_PROTOCOL_NMEA 0U /* the only protocol implemented -- UBX is a
                                 real future value (issue #56's own "out
                                 of scope" note), not built yet; gps.c
                                 treats any other value as unsupported,
                                 same FAILED-forever treatment as an
                                 unassigned port */

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

/* Bench-diagnostic counters, same "tell link-dead apart from link-alive-
   but-not-decoding apart from genuinely-working" split
   lib/telemetry/sport.c's own sport_get_counters() already provides --
   bytesReceived proves the physical UART link + baud are alive at all
   (every byte gps_process_byte() ever sees, regardless of NMEA
   validity); validSentences proves real, checksum-correct NMEA is
   arriving, independent of whether a fix currently exists (gps_lat/etc.
   only ever update from a GGA/RMC sentence that's already counted here
   first). Lets `diag gps` answer "is the GPS module talking at all" on
   a bench with no sky view to ever produce a real fix. */
void gps_get_counters(uint32_t *bytesReceived, uint32_t *validSentences);

#endif /* HELM_GPS_H */
