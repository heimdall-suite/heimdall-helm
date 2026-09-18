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

/* TODO: rx_poll() (the eventual RX task, writer) and rx_get_latest() (any
   other task, reader) will need a thread-safety mechanism (mutex or
   similar) once a real task actually calls poll() concurrently with
   readers -- not added yet, this is structure only. */

/* Binds the active driver and calls its init(). Selection is the
   persisted input-mode param once HELM_FEATURE_PARAMS_PERSIST lands
   (#10); until then, the compile-time HELM_RX_DEFAULT_PROTOCOL_SBUS/
   _CRSF default in board_features.h. */
void rx_start(void);

/* Polls the currently-bound driver, storing its output for
   rx_get_latest(). Not yet called from a task of its own -- that lands
   with the real Input->Mapping->Control->Output chain (#7). */
void rx_poll(void);

void rx_get_latest(RxFrame *out);

#endif /* HELM_RX_H */
