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

static QueueHandle_t mapping_queue;

static void mapping_task(void *arg) {
    (void)arg;

    /* Registers from this task's own context, not mapping_start() -- see
       supervisor_register()'s header comment: concurrent module tasks
       registering after the scheduler starts is exactly the race its
       critical section protects against. */
    MappingFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    SupervisorHandle handle = supervisor_register("mapping", mapping_queue, &fallback,
                                                   sizeof(fallback),
                                                   pdMS_TO_TICKS(MAPPING_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(MAPPING_TASK_PERIOD_MS));

        RxFrame in;
        rx_get_latest(&in);

        /* Passthrough stand-in -- see mapping.h for why this is
           structurally identical to the RX frame it read. */
        MappingFrame out;
        memcpy(out.channels, in.channels, sizeof(out.channels));
        out.status = in.status;

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
    xQueueOverwrite(mapping_queue, &initial);

    xTaskCreate(mapping_task, "mapping", configMINIMAL_STACK_SIZE, NULL,
                MAPPING_TASK_PRIORITY, NULL);
}

void mapping_get_latest(MappingFrame *out) {
    xQueuePeek(mapping_queue, out, 0);
}
