#include "rx.h"

#include "board_features.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"

/* Module-level glue, not per-protocol -- binds whichever driver is
   active and forwards to it. See .docs/architecture/module-architecture.md's
   RX case study: which driver is active is a runtime pick, not a build-
   time one, so this file (unlike sbus.c/crsf.c) has no protocol of its
   own. */

/* Placeholder task period/priority -- pure plumbing stub (issue #7), not
   tuned against any real timing requirement yet: module-architecture.md's
   priority tiers are still an open decision, and the active driver's
   poll() only returns fixed test data so far (real decode is #8/#9).
   Every stage in the Input->Mapping->Control->Output->Servo chain uses
   the same placeholder numbers for now. */
#define RX_TASK_PERIOD_MS 20
#define RX_TASK_PRIORITY 1

static const rx_driver_t *active_driver;
static QueueHandle_t rx_queue;

void rx_poll(void) {
    RxFrame frame;
    active_driver->poll(&frame);
    xQueueOverwrite(rx_queue, &frame);
}

static void rx_task(void *arg) {
    (void)arg;

    /* Registers from this task's own context, not rx_start() -- see
       supervisor_register()'s header comment: concurrent module tasks
       registering after the scheduler starts is exactly the race its
       critical section protects against. */
    RxFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    SupervisorHandle handle = supervisor_register("rx", rx_queue, &fallback, sizeof(fallback),
                                                   pdMS_TO_TICKS(RX_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(RX_TASK_PERIOD_MS));
        rx_poll();
        supervisor_kick(handle);
    }
}

void rx_start(void) {
#if HELM_RX_DEFAULT_PROTOCOL_CRSF
    active_driver = rx_crsf_driver();
#elif HELM_RX_DEFAULT_PROTOCOL_SBUS
    active_driver = rx_sbus_driver();
#else
#error "board_features.h must set exactly one of HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF"
#endif

    active_driver->init();

    rx_queue = xQueueCreate(1, sizeof(RxFrame));

    /* Seed the queue before anything downstream can peek it -- a
       length-1 overwrite queue only holds a valid value once something
       has actually written to it once. */
    rx_poll();

    xTaskCreate(rx_task, "rx", configMINIMAL_STACK_SIZE, NULL, RX_TASK_PRIORITY, NULL);
}

void rx_get_latest(RxFrame *out) {
    xQueuePeek(rx_queue, out, 0);
}
