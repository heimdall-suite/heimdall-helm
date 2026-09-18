#ifndef HELM_SUPERVISOR_H
#define HELM_SUPERVISOR_H

#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

/* Generic module-liveness supervisor -- generalizes the OK/FAILSAFE
   status-substitution mechanism .docs/architecture/receiver-to-servo.md
   designs for RX to any module, per .docs/architecture/
   module-architecture.md's "Fault isolation" section.

   A module publishes its output as {value, status} into its own
   length-1 queue via xQueueOverwrite (see that doc's "Inter-stage data"
   section) and registers that queue here with a safe fallback value and
   an expected max update period. The module's own task then calls
   supervisor_kick() once per pass -- proving the TASK is still
   scheduling and running, independent of whether its DATA changed (a
   module with nothing new to report still kicks; only a task that's
   stopped executing entirely -- wedged, crashed, blocked forever --
   stops kicking). This is a different failure mode than a driver's own
   data-level timeout (e.g. lib/rx/shared/rx_timeout.c detecting "no
   valid frames arriving") -- both can independently drive the same
   OK/FAILSAFE status, for different reasons.

   If a registered module hasn't kicked within its declared period, the
   supervisor overwrites that module's OWN output queue with its
   registered fallback value -- the exact substitution a dead RX link
   already triggers via its own timeout, just supervisor-triggered here.
   Downstream consumers can't tell the difference, and don't need to. */

/* Cap on a module's {value,status} struct size the supervisor can hold
   as a fallback. Raise if a real module's output struct grows past
   this -- fixed-size by design, no dynamic allocation for the
   registration table (this table has to stay usable even under heap
   pressure that might be exactly the kind of thing a module wedges
   on). */
#define SUPERVISOR_MAX_VALUE_BYTES 64

/* Fixed upper bound on registered modules -- same fixed-size-table
   reasoning as SUPERVISOR_MAX_VALUE_BYTES. */
#define SUPERVISOR_MAX_MODULES 8

typedef int SupervisorHandle;
#define SUPERVISOR_INVALID_HANDLE (-1)

/* Registers a module with the supervisor.
   - `output_queue`: the module's own length-1 latest-value queue,
     already created by the caller via xQueueCreate(1, sizeof(...)).
   - `fallback_value`/`fallback_size`: a safe substitute value, copied by
     value now (not referenced later) -- safe to point at a stack local.
     `fallback_size` must exactly match `output_queue`'s configured item
     size (the supervisor has no way to query that itself) and must fit
     within SUPERVISOR_MAX_VALUE_BYTES.
   - `max_period_ticks`: how long the module has after this call (or its
     last supervisor_kick()) before it's treated as wedged.

   Returns a handle to pass to supervisor_kick(), or
   SUPERVISOR_INVALID_HANDLE if the table is full or fallback_size is too
   large -- callers should treat either as a build-time-visible
   configuration bug (raise the relevant cap above), not a runtime
   condition to recover from. */
SupervisorHandle supervisor_register(const char *name, QueueHandle_t output_queue,
                                      const void *fallback_value, size_t fallback_size,
                                      TickType_t max_period_ticks);

/* Call once per pass from the registered module's own task, whether or
   not it produced fresh output this pass. A no-op on an invalid
   handle. */
void supervisor_kick(SupervisorHandle handle);

/* Starts the supervisor task. Call once from main(), after board_init()
   and before any module that will register with it -- unconditional,
   not gated behind a HELM_FEATURE_* flag, same as
   debug_heartbeat_start(): every board gets the safety net regardless of
   feature budget. */
void supervisor_start(void);

#endif /* HELM_SUPERVISOR_H */
