#ifndef HELM_TELEMETRY_H
#define HELM_TELEMETRY_H

#include <stdint.h>

/* Protocol-agnostic telemetry core (issue #16) -- a small static table
   of named fields, meant to be kept fresh by a gather task pulling from
   each source's own latest-value queue (issue #17 adds the first real
   pull; this issue is just the table + API + task scaffolding). Real
   link-quality/RSSI is NOT one of these fields -- the receiver reports
   that itself, on both protocols; see .docs/architecture/telemetry.md's
   "Link quality is not gathered here" section. Protocol-specific
   adapters (S.Port issue #18, CRSF issue #19) only ever read this table
   via telemetry_get() -- they never reach into a sensor/RX queue
   directly. */

typedef enum {
    TELEM_STATUS_OK,
    TELEM_STATUS_STALE,  /* a previous value exists but hasn't been
                             refreshed recently -- not yet auto-detected
                             by age (needs a real per-field refresh
                             interval to compare against, see
                             .docs/architecture/telemetry.md's "Not yet
                             decided"); currently only set by whoever
                             calls telemetry_set(). */
    TELEM_STATUS_FAILED, /* source reported FAILED/absent. */
} TelemetryStatus;

typedef enum {
    /* Placeholder only -- issue #17's synthetic test value, proving the
       table/gather/adapter chain mechanically before any real producer
       (sensors, etc.) exists. Kept alongside the real fields below, not
       replaced by them -- same "keep the proof value" precedent
       lib/params/params.h's PARAM_TEST_COUNTER set. */
    TELEM_FIELD_TEST,

    /* Issue #33 -- real values, pulled from lib/sensors/baro.h's
       baro_get_latest() by the gather task (telemetry.c), only on boards
       with HELM_HAS_BARO set. FAILED (not STALE/zeroed) on any board
       without a baro, or if the chip's own read fails -- same
       never-fabricate-a-value discipline baro.h's own comment
       describes. */
    TELEM_FIELD_BARO_PRESSURE,    /* station pressure, Pa */
    TELEM_FIELD_BARO_TEMPERATURE, /* chip's own die temperature, deg C */

    TELEM_FIELD_COUNT,
} TelemetryField;

typedef struct {
    float value;
    TelemetryStatus status;
    uint32_t last_updated_ms;
} TelemetryEntry;

/* Starts the gather task. Call once from main(), gated on
   HELM_FEATURE_TELEMETRY. No supervisor registration -- best-effort
   tier, same as debug_heartbeat_start()/cli_start(): a stalled gather
   task means stale telemetry, not something worth an IWDG reset over,
   and its per-field table doesn't fit supervisor_register()'s
   one-queue-one-fallback shape anyway (staleness is already visible per
   field via last_updated_ms instead). */
void telemetry_start(void);

/* Writes field's latest value -- called by the gather task as it pulls
   from a real source (once one exists), or directly by a producer with
   nothing to peek from yet (issue #17's synthetic value). Last writer
   wins, same latest-value semantics as everywhere else in this
   codebase. Safe from any task context. */
void telemetry_set(TelemetryField field, float value, TelemetryStatus status);

/* Reads field's current entry -- the only way a protocol adapter
   touches telemetry content; adapters never read a sensor/RX queue
   directly. */
void telemetry_get(TelemetryField field, TelemetryEntry *out);

#endif /* HELM_TELEMETRY_H */
