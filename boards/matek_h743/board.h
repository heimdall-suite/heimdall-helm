#ifndef HELM_BOARD_MATEK_H743_H
#define HELM_BOARD_MATEK_H743_H

#define HELM_BOARD_NAME "matek_h743"

/* Called once from main.c, after HAL_Init(), before the scheduler starts.
   Brings up this board's clock tree and any other early, board-specific
   peripheral init that has to happen before modules/drivers touch
   hardware. */
void board_init(void);

/* Toggles the onboard "CAL" LED (PE3) -- pin/polarity ported from this
   project's sibling aoa-boat-controller (include/pins_h743.h: PIN_CAL_LED
   PE3, active-low, confirmed against this unit's live Betaflight
   `resource` readout: `LED 1 E03`). Used by lib/debug/heartbeat.c as a
   visible "is the scheduler alive" signal, gated on this board's
   HELM_HAS_DEBUG_LED. */
void board_led_toggle(void);

#endif /* HELM_BOARD_MATEK_H743_H */
