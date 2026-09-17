#include "rx_timeout.h"

/* Structure only -- no logic yet. */

void rx_timeout_init(RxTimeoutWatchdog *watchdog, uint32_t timeout_ms) {
    (void)watchdog;
    (void)timeout_ms;
}

void rx_timeout_note_valid_frame(RxTimeoutWatchdog *watchdog) {
    (void)watchdog;
}

bool rx_timeout_has_expired(const RxTimeoutWatchdog *watchdog) {
    (void)watchdog;
    return false;
}
