#include "control.h"

#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"
#include "mapping.h"

/* See mapping.c's own comment -- same placeholder-period-and-priority
   reasoning applies to every stage in this chain (issue #7). */
#define CONTROL_TASK_PERIOD_MS 20
#define CONTROL_TASK_PRIORITY 1

static QueueHandle_t control_queue;

static void control_task(void *arg) {
    (void)arg;

    ControlFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    SupervisorHandle handle = supervisor_register("control", control_queue, &fallback,
                                                   sizeof(fallback),
                                                   pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(CONTROL_TASK_PERIOD_MS));

        MappingFrame in;
        mapping_get_latest(&in);

        ControlFrame out;
        memcpy(out.channels, in.channels, sizeof(out.channels));
        out.status = in.status;

        /* Issue #38: Off produces no *correction*, but the mapped raw
           target still flows through to output mapping's own endpoint/
           subtrim/direction trim -- same placeholder body
           (straight passthrough of pitchTarget, not real PID/attitude
           math, see #35) regardless of mode, since there's no actual
           control law yet to turn off. pitchActive only ever goes false
           for a genuine RX failsafe, which mapping.c already handles by
           forcing pitchTarget to its own safe centered setpoint before
           this stage ever sees it (mapping.c's own comment) -- so this
           stage needs zero failsafe-awareness of its own, same reasoning
           the old code already relied on, just no longer gated on mode.
           control-loops.md's "Off produces no output" line has been
           updated to match: no *correction*, not "no output at all".

           pitchActive tracks RX status, NOT pitchMode -- mirrors
           output.c's own passthrough slots (sourceValid = mapping.status
           == RX_STATUS_OK). Needed so a genuine RX failsafe still goes
           false here and falls through to output.c's configured
           failsafe policy (HOLD/FIXED) for the pitch slot, exactly as
           before this issue -- only Off-vs-Active stopped mattering,
           OK-vs-FAILSAFE still does. */
        out.pitchActive = (in.status == RX_STATUS_OK);
        out.pitchOutput = in.pitchTarget;

        xQueueOverwrite(control_queue, &out);
        supervisor_kick(handle);
    }
}

void control_start(void) {
    control_queue = xQueueCreate(1, sizeof(ControlFrame));

    ControlFrame initial = {0};
    initial.status = RX_STATUS_FAILSAFE;
    xQueueOverwrite(control_queue, &initial);

    xTaskCreate(control_task, "control", configMINIMAL_STACK_SIZE, NULL,
                CONTROL_TASK_PRIORITY, NULL);
}

void control_get_latest(ControlFrame *out) {
    xQueuePeek(control_queue, out, 0);
}
