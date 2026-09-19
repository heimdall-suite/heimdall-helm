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

/* IWDG (STM32F1's independent, LSI-clocked watchdog) -- issue #11,
   porting #5's matek_h743 (STM32H7) IWDG to this board. Called exactly
   once, from lib/supervisor/supervisor.c's task on its own first pass
   (gated on HELM_HAS_IWDG): starting it is a one-way door in hardware
   (no software disable exists once running), so it must not start
   before something is actually committed to feeding it. See board.c for
   the prescaler/reload derivation, which differs from H7's despite the
   register shape being the same -- this chip's LSI runs at a different
   nominal frequency. */
void board_iwdg_init(void);

/* Feeds (reloads) the running IWDG counter -- called once per pass from
   the same supervisor task, after board_iwdg_init(). Must never be
   called from anywhere else: the entire safety property depends on
   this only happening when the supervisor's own loop is still actually
   scheduling, not on demand. */
void board_iwdg_refresh(void);

#endif /* HELM_BOARD_AFROFLIGHT32_H */
