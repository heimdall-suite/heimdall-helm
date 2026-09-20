#ifndef HELM_RX_H
#define HELM_RX_H

#include <stdint.h>

/* Common interface every RX protocol driver implements (sbus.c, crsf.c --
   both compile in on every board, see lib/README.md's addendum and
   .docs/architecture/module-architecture.md's RX case study for why this
   is a runtime vtable pick rather than the usual build-time single-pick
   template). See .docs/architecture/receiver-to-servo.md's Failsafe
   section for the reasoning behind the output shape, in particular why
   status is only two tiers. */

#define RX_MAX_CHANNELS 16

typedef enum {
    RX_STATUS_OK,       /* covers both a fresh frame and a currently-stale-
                            but-not-yet-timed-out one -- both look the same
                            to a consumer: use what's in the struct. */
    RX_STATUS_FAILSAFE, /* latched loss -- see rx_timeout.h and each
                            driver's own detection (protocol bits and/or
                            the shared timeout watchdog). */
} RxStatus;

typedef struct {
    uint16_t channels[RX_MAX_CHANNELS];
    RxStatus status;

    /* Diagnostic only -- for logging/telemetry (link-quality, frame-loss
       rate), not control-relevant. Nothing in the mapping stage or below
       should branch on these. */
    uint32_t last_frame_age_ms;
    uint32_t frame_loss_count;
} RxFrame;

/* One vtable per protocol driver -- {init, poll} function pointers, not
   #ifdef-selected free functions, so both sbus.c and crsf.c can compile
   in unconditionally without colliding at link time. rx_sbus_driver()/
   rx_crsf_driver() each just return a pointer to their own static
   instance of this. */
typedef struct {
    void (*init)(void);
    void (*poll)(RxFrame *out);
} rx_driver_t;

const rx_driver_t *rx_sbus_driver(void);
const rx_driver_t *rx_crsf_driver(void);

/* Input-mode param encoding (issue #10) -- persisted as
   lib/params/params.h's PARAM_INPUT_MODE when HELM_FEATURE_PARAMS_PERSIST
   is on. Plain values, not an enum: params.h's ParamType only stores u32,
   so this keeps rx_start()'s comparison and params.c's own per-board
   default (derived from HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF below)
   trivial on both sides. */
#define RX_INPUT_MODE_SBUS 0U
#define RX_INPUT_MODE_CRSF 1U

/* Binds the active driver, calls its init(), and starts the RX task
   (issue #7's Input stage) that periodically calls rx_poll() and
   publishes into RX's own length-1 supervised queue. Driver selection
   (issue #10): the persisted PARAM_INPUT_MODE value when
   HELM_FEATURE_PARAMS_PERSIST is on, else the compile-time
   HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF default in board_features.h --
   that default also seeds the param's own factory value (params.c), so a
   freshly-flashed/never-written board still boots into the protocol its
   wiring expects, not a hardcoded pick. */
void rx_start(void);

/* Polls the currently-bound driver and publishes its output into RX's
   queue (xQueueOverwrite) for rx_get_latest() (xQueuePeek) -- see
   .docs/architecture/module-architecture.md's "Inter-stage data" section
   for why overwrite/peek rather than a normal queue. Exposed publicly so
   rx_start() can seed the queue with one real poll before the RX task's
   own loop starts; the task is the only other caller. */
void rx_poll(void);

void rx_get_latest(RxFrame *out);

#endif /* HELM_RX_H */
