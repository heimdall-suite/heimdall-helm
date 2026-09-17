#include "rx.h"
#include "shared/rx_timeout.h"

/* SBUS driver: structure only, no decode logic yet. Will own a UART RX
   interrupt/DMA path decoding SBUS frames, its own RxTimeoutWatchdog as a
   backstop, and SBUS's own explicit frame-lost/failsafe bits as the fast
   path -- see .docs/architecture.md's Failsafe section. */

static RxFrame latest = {0};
static RxTimeoutWatchdog timeout_watchdog;

void rx_init(void) {
    rx_timeout_init(&timeout_watchdog, 0);
}

void rx_update(void) {
}

void rx_get_latest(RxFrame *out) {
    *out = latest;
}
