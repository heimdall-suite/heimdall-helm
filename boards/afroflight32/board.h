#ifndef HELM_BOARD_AFROFLIGHT32_H
#define HELM_BOARD_AFROFLIGHT32_H

#define HELM_BOARD_NAME "afroflight32"

/* Called once from main.c, after HAL_Init(), before the scheduler starts.
   Brings up this board's clock tree and any other early, board-specific
   peripheral init that has to happen before modules/drivers touch
   hardware. */
void board_init(void);

/* Toggles the onboard "CAL" LED (PB4, cleanflight's LED1_PIN on this
   board) -- pin/polarity ported from aoa-boat-controller's real,
   currently-running firmware for this exact physical board
   (include/pins_naze32.h: `#define PIN_CAL_LED PB4`, active-LOW --
   "board sinks current through them to light", confirmed on real
   hardware at that project's original bring-up). PB4 is JTAG-shared
   (NJTRST) but freed by board.c's own AFIO SWJ-NOJTAG remap
   (system_clock_config(), already needed there for an unrelated pin).
   Used by lib/debug/heartbeat.c as a visible "is the scheduler alive"
   signal, gated on this board's HELM_HAS_DEBUG_LED. */
void board_led_toggle(void);

#endif /* HELM_BOARD_AFROFLIGHT32_H */
