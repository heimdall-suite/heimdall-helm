#include "telemetry.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

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
