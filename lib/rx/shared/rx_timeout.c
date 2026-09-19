#include "rx_timeout.h"

#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS-tick-based backstop -- see rx_timeout.h's own header comment
   for why this exists alongside SBUS's in-frame failsafe bit rather than
   instead of it. Tick subtraction wraps correctly even across
   xTaskGetTickCount() rollover: TickType_t is unsigned, so
   (now - last_valid_frame_tick) is the true elapsed tick count regardless
   of which side of a rollover either value falls on. */

void rx_timeout_init(RxTimeoutWatchdog *watchdog, uint32_t timeout_ms) {
    watchdog->timeout_ms = timeout_ms;
    watchdog->last_valid_frame_tick = xTaskGetTickCount();
}

void rx_timeout_note_valid_frame(RxTimeoutWatchdog *watchdog) {
    watchdog->last_valid_frame_tick = xTaskGetTickCount();
}

bool rx_timeout_has_expired(const RxTimeoutWatchdog *watchdog) {
    TickType_t const elapsed = xTaskGetTickCount() - watchdog->last_valid_frame_tick;
    return elapsed >= pdMS_TO_TICKS(watchdog->timeout_ms);
}
