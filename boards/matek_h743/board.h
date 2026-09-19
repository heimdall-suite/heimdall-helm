#ifndef HELM_BOARD_MATEK_H743_H
#define HELM_BOARD_MATEK_H743_H

#include <stdbool.h>
#include <stdint.h>

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

/* SBUS's own fixed frame length (header + 22 payload + flags + footer) --
   a protocol fact, not a board one, but defined here since it's the fixed
   contract of the board_sbus_uart_* buffer interface below. Kept as one
   shared constant (rather than a number hand-copied into both this file
   and lib/rx/sbus.c) specifically to avoid the kind of silent staleness
   risk FreeRTOSConfig.h's own configCPU_CLOCK_HZ comment already flags
   for a similar hand-maintained-in-two-places value. */
#define SBUS_UART_FRAME_LEN 25

/* USART6 RX-only, PC7 (silk-labeled "RX6" on this board) -- confirmed via
   this exact unit's own Betaflight target (manufacturer_id MTKS, board_name
   MATEKH743, github.com/betaflight/unified-targets configs/default/
   MTKS-MATEKH743.config): `resource SERIAL_RX 6 C07` + `set
   serialrx_provider = SBUS`, cross-checked against the physical wiring
   (SBUS receiver signal wired to the RX6 pad this session). No external/
   onboard inverter: that same Betaflight config has no `resource
   INVERTER` line for this target (unlike older F1/F3 boards that need
   one), and aoa-boat-controller's lib/Sbus/H743/SbusReceiver.h confirms
   directly -- STM32H7 has a genuine hardware RX-invert bit
   (USART_CR2_RXINV, confirmed present in stm32h7xx_hal_uart.h) -- so
   board_sbus_uart_init() below enables that instead of wiring a GPIO-
   gated inverter chip.

   Sets up USART6 + DMA1 Stream0 for HAL_UARTEx_ReceiveToIdle_DMA
   (one-shot-with-idle-line-detection, re-armed after each completed or
   discarded capture) rather than a byte interrupt: DMA moves each byte
   out of the peripheral in hardware with no per-byte ISR involvement, so
   capture is robust against FreeRTOS task/interrupt jitter in a way a
   126us-per-byte re-armed byte interrupt (12 bits/byte at 100000 baud)
   would not be, now that this board runs several other tasks (see #7).
   DMA1 Stream0 is an arbitrary pick -- nothing else on this board uses
   DMA yet. */
void board_sbus_uart_init(void);

/* Copies the most recently completed SBUS_UART_FRAME_LEN-byte capture
   into `out` and returns true, or returns false (out untouched) if
   nothing new has completed since the last call -- lib/rx/sbus.c's
   sbus_poll() pulls at its own rate rather than being pushed to, per
   .docs/architecture/module-architecture.md's "pull, not push" inter-
   stage convention, applied here one layer below the module boundary
   too. A capture that comes up short (idle line fired before
   SBUS_UART_FRAME_LEN bytes arrived -- noise, a dropped byte, or startup
   mid-frame) is silently discarded here, never handed up: the next
   capture starts right after a real inter-frame gap, so this is already
   self-resynchronizing without sbus.c needing any byte-level resync
   state machine of its own. */
bool board_sbus_uart_take_frame(uint8_t out[SBUS_UART_FRAME_LEN]);

/* IWDG1 (STM32H7's independent, LSI-clocked watchdog) -- issue #5,
   .docs/architecture/module-architecture.md's "Crash safety" section.
   Called exactly once, from lib/supervisor/supervisor.c's task on its
   own first pass (gated on HELM_HAS_IWDG): starting it is a one-way
   door in hardware (no software disable exists once running), so it
   must not start before something is actually committed to feeding it.
   See board.c for the prescaler/reload derivation. */
void board_iwdg_init(void);

/* Feeds (reloads) the running IWDG counter -- called once per pass from
   the same supervisor task, after board_iwdg_init(). Must never be
   called from anywhere else: the entire safety property depends on
   this only happening when the supervisor's own loop is still actually
   scheduling, not on demand. */
void board_iwdg_refresh(void);

#endif /* HELM_BOARD_MATEK_H743_H */
