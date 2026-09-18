#include "rx.h"

#include "board_features.h"

/* Module-level glue, not per-protocol -- binds whichever driver is
   active and forwards to it. See .docs/architecture/module-architecture.md's
   RX case study: which driver is active is a runtime pick, not a build-
   time one, so this file (unlike sbus.c/crsf.c) has no protocol of its
   own. */

static const rx_driver_t *active_driver;
static RxFrame latest = {0};

void rx_start(void) {
#if HELM_RX_DEFAULT_PROTOCOL_CRSF
    active_driver = rx_crsf_driver();
#elif HELM_RX_DEFAULT_PROTOCOL_SBUS
    active_driver = rx_sbus_driver();
#else
#error "board_features.h must set exactly one of HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF"
#endif

    active_driver->init();
}

void rx_poll(void) {
    active_driver->poll(&latest);
}

void rx_get_latest(RxFrame *out) {
    *out = latest;
}
