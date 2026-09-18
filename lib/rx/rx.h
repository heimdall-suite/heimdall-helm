#ifndef HELM_RX_H
#define HELM_RX_H

#include <stdint.h>

/* Common interface every RX protocol driver implements (sbus.c, crsf.c --
   exactly one compiled per board, see scripts/add_rx.py and
   platformio.ini's custom_helm_rx). See .docs/architecture/
   receiver-to-servo.md's Failsafe section for the reasoning behind this
   shape, in particular why status is only two tiers. */

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

/* TODO: rx_update() (the RX task, writer) and rx_get_latest() (any other
   task, reader) will need a thread-safety mechanism (mutex or similar)
   once both are actually implemented concurrently -- not added yet, this
   is structure only. */

void rx_init(void);
void rx_update(void);
void rx_get_latest(RxFrame *out);

#endif /* HELM_RX_H */
