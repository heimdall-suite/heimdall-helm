#include "supervisor.h"

#include <stdbool.h>
#include <string.h>
#include "task.h"

/* Poll period for the supervisor's own liveness sweep. Deliberately
   short relative to the kind of max_period_ticks a time-critical-I/O
   module would register (tens of ms) so detection latency stays well
   under a module's own declared budget -- not yet tuned against a real
   module's numbers, see module-architecture.md's "Open / not yet
   decided" list. */
#define SUPERVISOR_POLL_PERIOD_MS 10

/* Placeholder priority -- module-architecture.md leaves the supervisor's
   exact tier as an open decision in its own priority-tiers section.
   Picked above the "best-effort" tier (telemetry/logging/CLI/heartbeat,
   currently priority 1 for all of them) since a stalled safety mechanism
   is worse than a stalled log line, but below where time-critical I/O
   will eventually land -- revisit once real modules with real priorities
   exist to compare against. */
#define SUPERVISOR_TASK_PRIORITY 2

typedef struct {
    const char *name;
    QueueHandle_t output_queue;
    uint8_t fallback_value[SUPERVISOR_MAX_VALUE_BYTES];
    TickType_t max_period_ticks;
    volatile TickType_t last_kick_tick;
    volatile bool in_use;
} SupervisorEntry;

static SupervisorEntry entries[SUPERVISOR_MAX_MODULES];

SupervisorHandle supervisor_register(const char *name, QueueHandle_t output_queue,
                                      const void *fallback_value, size_t fallback_size,
                                      TickType_t max_period_ticks) {
    if (fallback_size > SUPERVISOR_MAX_VALUE_BYTES) {
        return SUPERVISOR_INVALID_HANDLE;
    }

    SupervisorHandle handle = SUPERVISOR_INVALID_HANDLE;

    /* The table insert has to be atomic against another module task
       registering concurrently -- every module registers from its own
       task context after the scheduler starts, not serialized by main()
       the way initial task creation is. */
    taskENTER_CRITICAL();
    for (int i = 0; i < SUPERVISOR_MAX_MODULES; i++) {
        if (!entries[i].in_use) {
            entries[i].name = name;
            entries[i].output_queue = output_queue;
            memcpy(entries[i].fallback_value, fallback_value, fallback_size);
            entries[i].max_period_ticks = max_period_ticks;
            entries[i].last_kick_tick = xTaskGetTickCount();
            entries[i].in_use = true;
            handle = i;
            break;
        }
    }
    taskEXIT_CRITICAL();

    return handle;
}

void supervisor_kick(SupervisorHandle handle) {
    if (handle < 0 || handle >= SUPERVISOR_MAX_MODULES) {
        return;
    }
    /* A lone aligned 32-bit store is atomic on Cortex-M without a
       critical section -- the same assumption xTaskGetTickCount() itself
       relies on elsewhere in FreeRTOS, and why this doesn't need the
       same critical section supervisor_register()'s multi-field insert
       does. */
    entries[handle].last_kick_tick = xTaskGetTickCount();
}

static void supervisor_task(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SUPERVISOR_POLL_PERIOD_MS));

        TickType_t now = xTaskGetTickCount();
        for (int i = 0; i < SUPERVISOR_MAX_MODULES; i++) {
            if (!entries[i].in_use) {
                continue;
            }
            /* Unsigned subtraction wraps correctly across tick-count
               rollover -- standard FreeRTOS elapsed-time idiom, not a
               bug. */
            if ((TickType_t)(now - entries[i].last_kick_tick) > entries[i].max_period_ticks) {
                xQueueOverwrite(entries[i].output_queue, entries[i].fallback_value);
            }
        }
    }
}

void supervisor_start(void) {
    xTaskCreate(supervisor_task, "supervisor", configMINIMAL_STACK_SIZE, NULL,
                SUPERVISOR_TASK_PRIORITY, NULL);
}
