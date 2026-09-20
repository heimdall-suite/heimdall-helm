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

        if (in.pitchMode == PITCH_MODE_OFF) {
            /* control-loops.md: "A loop in Off mode produces no output."
               No failsafe-awareness needed here -- mapping.c already
               forces PitchMode to Off during failsafe (control.h's own
               comment). */
            out.pitchActive = false;
            out.pitchOutput = 0; /* meaningless while inactive; zeroed, not left garbage, before it goes out over the queue */
        } else {
            /* Placeholder body (issue #35) -- straight passthrough of
               the mapped target, not real PID/attitude math (a later
               issue, once control-loops.md's own rate/fusion questions
               are resolved). */
            out.pitchActive = true;
            out.pitchOutput = in.pitchTarget;
        }

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
