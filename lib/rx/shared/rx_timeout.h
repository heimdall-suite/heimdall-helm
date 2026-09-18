#ifndef HELM_RX_TIMEOUT_H
#define HELM_RX_TIMEOUT_H

#include <stdint.h>
#include <stdbool.h>

/* Shared receive-timeout watchdog, used internally by every RX driver --
   as the ONLY detection mechanism for protocols with no in-frame failsafe
   bit (CRSF), and as a backstop under faster bit-based detection for
   protocols that have one (SBUS) -- a cut wire or a crashed receiver
   means no more frames arrive at all, bit or no bit. See
   .docs/architecture/receiver-to-servo.md's Failsafe section. Not part of
   rx.h's public interface -- each driver owns its own watchdog instance
   internally. */

typedef struct {
    uint32_t last_valid_frame_tick;
    uint32_t timeout_ms;
} RxTimeoutWatchdog;

void rx_timeout_init(RxTimeoutWatchdog *watchdog, uint32_t timeout_ms);
void rx_timeout_note_valid_frame(RxTimeoutWatchdog *watchdog);
bool rx_timeout_has_expired(const RxTimeoutWatchdog *watchdog);

#endif /* HELM_RX_TIMEOUT_H */
