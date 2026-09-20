#include "rx.h"

#include "board_features.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"

#if HELM_FEATURE_PARAMS_PERSIST
#include "params.h"
#endif

/* Compile-time fallback (issue #6): also params.c's own factory default
   for PARAM_INPUT_MODE (issue #10), so both paths agree on what a board
   without a persisted override -- or without params-persist at all --
   boots into. */
#if HELM_RX_DEFAULT_PROTOCOL_SBUS && HELM_RX_DEFAULT_PROTOCOL_CRSF
#error "board_features.h must set exactly one of HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF"
#elif HELM_RX_DEFAULT_PROTOCOL_CRSF
#define RX_DEFAULT_INPUT_MODE RX_INPUT_MODE_CRSF
#elif HELM_RX_DEFAULT_PROTOCOL_SBUS
#define RX_DEFAULT_INPUT_MODE RX_INPUT_MODE_SBUS
#else
#error "board_features.h must set exactly one of HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF"
#endif

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
#if HELM_FEATURE_PARAMS_PERSIST
    /* Runtime pick (issue #10): the persisted input-mode param, read
       once at container-start time -- not re-checked per poll, same
       "bind once, forward every tick" shape this module already had.
       param_get_u32() only fails on an out-of-range id, which
       PARAM_INPUT_MODE never is, so mode's RX_DEFAULT_INPUT_MODE
       initializer is unreachable in practice, not a real fallback path;
       it's there so a future misuse of this pattern fails safe instead
       of reading uninitialized stack. */
    uint32_t mode = RX_DEFAULT_INPUT_MODE;
    param_get_u32(PARAM_INPUT_MODE, &mode);
    active_driver = (mode == RX_INPUT_MODE_CRSF) ? rx_crsf_driver() : rx_sbus_driver();
#else
    active_driver =
        (RX_DEFAULT_INPUT_MODE == RX_INPUT_MODE_CRSF) ? rx_crsf_driver() : rx_sbus_driver();
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
