#include "mapping.h"

#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"

/* Placeholder task period/priority -- pure plumbing stub (issue #7), not
   tuned against any real timing requirement yet: module-architecture.md's
   priority tiers are still an open decision, and nothing in this stage
   does time-critical work (a straight passthrough copy). Every stage in
   this chain uses the same placeholder numbers for now. */
#define MAPPING_TASK_PERIOD_MS 20
#define MAPPING_TASK_PRIORITY 1

/* Issue #34's first-pass hardcoded table. Which channel index drives
   which function is inherently a throwaway placeholder here, same as
   receiver-to-servo.md's own CH2/CH4 illustrative example -- a per-user,
   per-transmitter config choice, not a project-wide hardware fact, and
   this issue's own scope note already says so ("a first pass could
   hardcode a table to prove the mechanism before wiring it to param
   storage"). CH9 is just whichever real 3-position switch was on hand to
   bench-test the banding logic below against; it carries no more
   permanence than CH2 would have. Real per-user channel assignment is
   later, params-backed work -- not decided here. */
#define MAPPING_PITCH_MODE_CHANNEL_INDEX 8   /* CH9 */
#define MAPPING_PITCH_TARGET_CHANNEL_INDEX 3 /* CH4 */

/* This project's sbus.c decode was ported from bolderflight/sbus (that
   file's own header comment), whose raw 11-bit tick convention this
   project inherits unchanged: 172 min, 992 center, 1811 max (988/1500/
   2012us on the wire, standard `us = raw * 0.625 + 880` conversion).
   Bench-confirmed end to end (issue #34, matek_h743, live receiver
   bound) -- NOT which channel this happens to be wired to (that's the
   throwaway part, see above), but the raw-tick<->microsecond conversion
   and equal-thirds banding logic itself, against a genuine 3-position
   switch's real output: the Taranis's own display showed that switch's
   three positions as exactly 988/1500/2012us, and `diag mapping` read
   back raw 172/992/1811 -> OFF/LIMIT/ACTIVE for each position in turn,
   over the real USART6/PC7 SBUS wiring (board_features.h's own
   HELM_HAS_SBUS_UART comment) -- not just inferred from the
   transmitter's displayed values. Canonical min/center/max, not
   some arbitrary in-between value, so this equal-thirds banding classifies
   all three with wide margin regardless of exactly where the two
   boundaries fall. */
#define MAPPING_CHANNEL_RAW_MIN 172U
#define MAPPING_CHANNEL_RAW_MAX 1811U
#define MAPPING_CHANNEL_RAW_CENTER 992U

static PitchMode pitch_mode_from_raw(uint16_t raw) {
    uint16_t const span = MAPPING_CHANNEL_RAW_MAX - MAPPING_CHANNEL_RAW_MIN;
    uint16_t const lowBoundary = (uint16_t)(MAPPING_CHANNEL_RAW_MIN + span / 3U);
    uint16_t const highBoundary = (uint16_t)(MAPPING_CHANNEL_RAW_MIN + (span * 2U) / 3U);

    if (raw < lowBoundary) {
        return PITCH_MODE_OFF;
    }
    if (raw < highBoundary) {
        return PITCH_MODE_LIMIT;
    }
    return PITCH_MODE_ACTIVE;
}

static QueueHandle_t mapping_queue;

static void mapping_task(void *arg) {
    (void)arg;

    /* Registers from this task's own context, not mapping_start() -- see
       supervisor_register()'s header comment: concurrent module tasks
       registering after the scheduler starts is exactly the race its
       critical section protects against. */
    MappingFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    fallback.pitchMode = PITCH_MODE_OFF;
    fallback.pitchTarget = MAPPING_CHANNEL_RAW_CENTER;
    SupervisorHandle handle = supervisor_register("mapping", mapping_queue, &fallback,
                                                   sizeof(fallback),
                                                   pdMS_TO_TICKS(MAPPING_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MAPPING_TASK_PERIOD_MS));

        RxFrame in;
        rx_get_latest(&in);

        /* channels[] stays a straight passthrough copy regardless of
           status -- see mapping.h's own comment on why passthrough
           failsafe substitution doesn't happen here anymore (#36 owns
           it, per physical output slot). */
        MappingFrame out;
        memcpy(out.channels, in.channels, sizeof(out.channels));
        out.status = in.status;

        if (in.status == RX_STATUS_FAILSAFE) {
            /* Mode/target function failsafe substitution -- this stage's
               own job (mapping.h's header comment), unlike passthrough. */
            out.pitchMode = PITCH_MODE_OFF;
            out.pitchTarget = MAPPING_CHANNEL_RAW_CENTER;
        } else {
            out.pitchMode = pitch_mode_from_raw(in.channels[MAPPING_PITCH_MODE_CHANNEL_INDEX]);
            out.pitchTarget = in.channels[MAPPING_PITCH_TARGET_CHANNEL_INDEX];
        }

        xQueueOverwrite(mapping_queue, &out);
        supervisor_kick(handle);
    }
}

void mapping_start(void) {
    mapping_queue = xQueueCreate(1, sizeof(MappingFrame));

    /* Seed the queue before anything downstream can peek it -- a
       length-1 overwrite queue only holds a valid value once something
       has actually written to it once. */
    MappingFrame initial = {0};
    initial.status = RX_STATUS_FAILSAFE;
    initial.pitchMode = PITCH_MODE_OFF;
    initial.pitchTarget = MAPPING_CHANNEL_RAW_CENTER;
    xQueueOverwrite(mapping_queue, &initial);

    xTaskCreate(mapping_task, "mapping", configMINIMAL_STACK_SIZE, NULL,
                MAPPING_TASK_PRIORITY, NULL);
}

void mapping_get_latest(MappingFrame *out) {
    xQueuePeek(mapping_queue, out, 0);
}
