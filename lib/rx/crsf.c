#include "rx.h"
#include "shared/rx_timeout.h"

/* CRSF driver: structure only, no decode logic yet. No in-frame failsafe
   bit exists in this protocol (unlike SBUS) -- the shared
   RxTimeoutWatchdog is this driver's ONLY detection mechanism, not a
   backstop. See .docs/architecture/receiver-to-servo.md's Failsafe
   section. */

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
