#ifndef HELM_DEBUG_HEARTBEAT_H
#define HELM_DEBUG_HEARTBEAT_H

/* Bring-up/debug signal: toggles the board's debug LED once a second
   (board_led_toggle(), board.h) on boards where HELM_HAS_DEBUG_LED is
   set -- a visible "is the scheduler alive" heartbeat. A no-op task on
   boards without a bench-confirmed LED pin, rather than guessing one. */
void debug_heartbeat_start(void);

#endif /* HELM_DEBUG_HEARTBEAT_H */
