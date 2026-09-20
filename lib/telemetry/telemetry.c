#include "telemetry.h"

#include "board_features.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#if HELM_HAS_BARO
#include "baro.h"
#endif
#if HELM_HAS_BATTERY_SENSE
#include "battery.h"
#endif
#if HELM_HAS_GPS
#include "gps.h"
#endif

/* Placeholder task period -- pure plumbing stub (issue #16), same as
   every other stage's stub period (e.g. RX_TASK_PERIOD_MS): no real
   per-field refresh interval decided yet (.docs/architecture/
   telemetry.md's "Not yet decided"), and this task currently has
   nothing to gather (issue #17 adds the first real/synthetic source).
   Priority 1 -- best-effort tier, matching heartbeat/CLI (see
   telemetry.h's header comment on why this module skips supervisor
   registration). */
#define TELEMETRY_GATHER_PERIOD_MS 100
#define TELEMETRY_GATHER_PRIORITY 1

/* One length-1 overwrite/peek queue per field -- same latest-value
   primitive used everywhere else in this codebase (see
   .docs/architecture/module-architecture.md's "Inter-stage data"
   section), scaled per field instead of per module so one producer's
   update can never race a concurrent read of a different field. */
static QueueHandle_t field_queues[TELEM_FIELD_COUNT];

#if HELM_HAS_BARO || HELM_HAS_BATTERY_SENSE || HELM_HAS_GPS
/* Explicit switch, not a cast -- SensorStatus and TelemetryStatus share
   the same OK/STALE/FAILED tiers but are deliberately not the same type
   (sensors.h's own comment: sensor drivers and telemetry are independent
   producers/consumers, coupling their status enums would be accidental,
   not structural). No default case, so the compiler's -Wswitch flags it
   if sensors.h ever grows a tier this doesn't handle -- the trailing
   return past the switch is only there to give the function a defined
   result in that circumstance, not a silent intended fallback. */
static TelemetryStatus sensor_status_to_telemetry_status(SensorStatus status) {
    switch (status) {
    case SENSOR_STATUS_OK:
        return TELEM_STATUS_OK;
    case SENSOR_STATUS_STALE:
        return TELEM_STATUS_STALE;
    case SENSOR_STATUS_FAILED:
        return TELEM_STATUS_FAILED;
    }
    return TELEM_STATUS_FAILED;
}
#endif /* HELM_HAS_BARO || HELM_HAS_BATTERY_SENSE || HELM_HAS_GPS */

static void telemetry_gather_task(void *arg) {
    (void)arg;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_GATHER_PERIOD_MS));

        /* Issue #17's synthetic end-to-end proof: uptime in seconds, an
           unambiguous "is this alive" value that needs no real sensor
           to exist yet -- same purpose aoa-boat-controller's own
           heartbeat-sensor placeholder served for its first S.Port
           bench test. No real source to pull from otherwise (sensor
           issues add those); this is a direct telemetry_set() call, not
           an xQueuePeek of anything. */
        telemetry_set(TELEM_FIELD_TEST, (float)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000), TELEM_STATUS_OK);

#if HELM_HAS_BARO
        /* Issue #33's first real source -- baro_get_latest() is the
           module's own public pull accessor (same as every other
           module-architecture.md "_get_latest()" boundary), not a direct
           reach into baro.c's queue. Runs at this task's 100ms period,
           well under baro.c's own 1000ms sample rate -- most passes just
           re-read the same sample, harmless for latest-value semantics. */
        BaroSample baroSample;
        baro_get_latest(&baroSample);
        TelemetryStatus const baroStatus = sensor_status_to_telemetry_status(baroSample.status);
        telemetry_set(TELEM_FIELD_BARO_PRESSURE, baroSample.pressure_pa, baroStatus);
        telemetry_set(TELEM_FIELD_BARO_TEMPERATURE, baroSample.temperature_c, baroStatus);
#endif

#if HELM_HAS_BATTERY_SENSE
        BatterySample batterySample;
        battery_get_latest(&batterySample);
        TelemetryStatus const batteryStatus = sensor_status_to_telemetry_status(batterySample.status);
        telemetry_set(TELEM_FIELD_BATTERY_VOLTAGE, batterySample.voltage_v, batteryStatus);
        telemetry_set(TELEM_FIELD_BATTERY_CURRENT, batterySample.current_a, batteryStatus);
#endif

#if HELM_HAS_GPS
        GpsFix gpsFix;
        gps_get_latest(&gpsFix);
        TelemetryStatus const gpsStatus = sensor_status_to_telemetry_status(gpsFix.status);
        telemetry_set(TELEM_FIELD_GPS_LATITUDE, gpsFix.latitude_deg, gpsStatus);
        telemetry_set(TELEM_FIELD_GPS_LONGITUDE, gpsFix.longitude_deg, gpsStatus);
        telemetry_set(TELEM_FIELD_GPS_ALTITUDE, gpsFix.altitude_m, gpsStatus);
        telemetry_set(TELEM_FIELD_GPS_SPEED, gpsFix.speed_mps, gpsStatus);
        telemetry_set(TELEM_FIELD_GPS_SATELLITES, (float)gpsFix.satellites, gpsStatus);
#endif
    }
}

void telemetry_start(void) {
    TelemetryEntry initial = {0};
    initial.status = TELEM_STATUS_FAILED;

    for (int i = 0; i < TELEM_FIELD_COUNT; i++) {
        field_queues[i] = xQueueCreate(1, sizeof(TelemetryEntry));

        /* Seed every field before anything can peek it -- same reason
           every other module's queue gets an initial xQueueOverwrite in
           its own _start(), see e.g. mapping_start(). */
        xQueueOverwrite(field_queues[i], &initial);
    }

    xTaskCreate(telemetry_gather_task, "telemetry", configMINIMAL_STACK_SIZE, NULL,
                TELEMETRY_GATHER_PRIORITY, NULL);
}

void telemetry_set(TelemetryField field, float value, TelemetryStatus status) {
    TelemetryEntry entry;
    entry.value = value;
    entry.status = status;
    entry.last_updated_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    xQueueOverwrite(field_queues[field], &entry);
}

void telemetry_get(TelemetryField field, TelemetryEntry *out) {
    xQueuePeek(field_queues[field], out, 0);
}
