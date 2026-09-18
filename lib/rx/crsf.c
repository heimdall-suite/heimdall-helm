#include "rx.h"
#include "shared/rx_timeout.h"

/* CRSF driver: structure only, no decode logic yet -- poll() returns
   fixed test data until real decode lands (#9). No in-frame failsafe bit
   exists in this protocol (unlike SBUS) -- the shared RxTimeoutWatchdog
   is this driver's ONLY detection mechanism, not a backstop. See
   .docs/architecture/receiver-to-servo.md's Failsafe section. */

static RxTimeoutWatchdog timeout_watchdog;

static void crsf_init(void) {
    rx_timeout_init(&timeout_watchdog, 0);
}

static void crsf_poll(RxFrame *out) {
    *out = (RxFrame){0};
    out->status = RX_STATUS_OK;
}

static const rx_driver_t driver = {
    .init = crsf_init,
    .poll = crsf_poll,
};

const rx_driver_t *rx_crsf_driver(void) {
    return &driver;
}
