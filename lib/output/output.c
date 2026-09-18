#include "output.h"

#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"
#include "control.h"

/* See mapping.c's own comment -- same placeholder-period-and-priority
   reasoning applies to every stage in this chain (issue #7). */
#define OUTPUT_TASK_PERIOD_MS 20
#define OUTPUT_TASK_PRIORITY 1

static QueueHandle_t output_queue;

static void output_task(void *arg) {
    (void)arg;

    OutputFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    SupervisorHandle handle = supervisor_register("output", output_queue, &fallback,
                                                   sizeof(fallback),
                                                   pdMS_TO_TICKS(OUTPUT_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(OUTPUT_TASK_PERIOD_MS));

        ControlFrame in;
        control_get_latest(&in);

        /* Passthrough stand-in -- see output.h. */
        OutputFrame out;
        memcpy(out.servos, in.channels, sizeof(out.servos));
        out.status = in.status;

        xQueueOverwrite(output_queue, &out);
        supervisor_kick(handle);
    }
}

void output_start(void) {
    output_queue = xQueueCreate(1, sizeof(OutputFrame));

    OutputFrame initial = {0};
    initial.status = RX_STATUS_FAILSAFE;
    xQueueOverwrite(output_queue, &initial);

    xTaskCreate(output_task, "output", configMINIMAL_STACK_SIZE, NULL,
                OUTPUT_TASK_PRIORITY, NULL);
}

void output_get_latest(OutputFrame *out) {
    xQueuePeek(output_queue, out, 0);
}
