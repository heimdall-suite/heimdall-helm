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

        /* Passthrough stand-in -- see control.h. */
        ControlFrame out;
        memcpy(out.channels, in.channels, sizeof(out.channels));
        out.status = in.status;

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
