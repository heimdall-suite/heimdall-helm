#include "rx.h"
#include "shared/rx_timeout.h"

/* SBUS driver: structure only, no decode logic yet -- poll() returns
   fixed test data until real UART/DMA decode lands (#8). Will own a UART
   RX interrupt/DMA path decoding SBUS frames, its own RxTimeoutWatchdog
   as a backstop, and SBUS's own explicit frame-lost/failsafe bits as the
   fast path -- see .docs/architecture/receiver-to-servo.md's Failsafe
   section. */

static RxTimeoutWatchdog timeout_watchdog;

static void sbus_init(void) {
    rx_timeout_init(&timeout_watchdog, 0);
}

static void sbus_poll(RxFrame *out) {
    *out = (RxFrame){0};
    out->status = RX_STATUS_OK;
}

static const rx_driver_t driver = {
    .init = sbus_init,
    .poll = sbus_poll,
};

const rx_driver_t *rx_sbus_driver(void) {
    return &driver;
}
