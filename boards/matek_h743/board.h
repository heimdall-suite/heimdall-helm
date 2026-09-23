#ifndef HELM_BOARD_MATEK_H743_H
#define HELM_BOARD_MATEK_H743_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

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

/* S.Port UART -- UART7/PE8 (silk-labeled "TX7" on this board), single-
   wire half-duplex, register-level LL driver, no HAL_UART_HandleTypeDef
   involved for this peripheral at all. Ported from aoa-boat-controller's
   own SportUart (lib/SPort/H743/SportUart), the same bench-verified
   driver that got a real receiver responding on this exact board/pin --
   not a from-scratch derivation. Carries over several hard-won, bench-
   confirmed findings from that project's own bring-up (its own
   SportUart.cpp/decisions.md have the full history):

   - Push-pull output, NOT open-drain -- an earlier "open-drain is
     necessary" belief there turned out to be a measurement-chain
     artifact; push-pull (matching Betaflight's own IOCFG_AF_PP for a
     S.Port-capable TX pin) is what actually produced real, structured
     bus traffic on a validated capture.
   - Both TX and RX invert bits set -- S.Port is electrically inverted,
     same reasoning as SBUS's own RXINV bit (see board_sbus_uart_init()'s
     comment), just both directions here since this is a real bus, not
     RX-only.
   - HAL's own interrupt-driven UART transmit (HAL_UART_Transmit_IT) was
     bench-confirmed (logic analyzer) to silently drop multi-byte writes
     in back-to-back sends -- exactly what a stuffed S.Port frame needs.
     This driver bypasses HAL_UART_HandleTypeDef entirely for this
     peripheral and matches Betaflight's real mechanism instead
     (src/platform/STM32/serial_uart_ll.c): TXEIE armed once, a single
     ISR keeps re-firing on its own as the shift register empties,
     draining a plain ring buffer.
   - TE/RE are kept mutually exclusive (CR1 bits toggled, never both on),
     switching back to listening only once TC -- transmission genuinely
     complete, not just TXE/queue-empty -- confirms the last bit has
     actually left the shift register. Switching early would let the
     receiver's own transmitted bytes echo back into this board's RX path.

   Real spec margin (ArduPilot's AP_Frsky_SPort.cpp, cross-checked on
   aoa-boat-controller's own bench): ~11.65ms poll-to-poll period, ~1.38ms
   per frame, leaving ~6.5ms of response margin -- FreeRTOS task-
   notification wake latency (this driver's ISR notifies a task rather
   than running protocol logic itself, see board_sport_uart_set_rx_task())
   is comfortably within that, same order of margin aoa-boat-controller's
   own bench measurement (5us reaction latency, ~1300x the required
   margin) already confirmed for a materially similar dispatch path. */

/* Registers the task this driver's ISR notifies (vTaskNotifyGiveFromISR)
   on every received byte -- call once, before board_sport_uart_init(),
   from the S.Port module's own start function. The ISR itself does no
   protocol logic (RXNE just fills a ring buffer and notifies); poll
   detection and frame handling happen in the notified task, keeping
   FreeRTOS API use (telemetry_get() and the queue read underneath it)
   in task context, never ISR context. */
void board_sport_uart_set_rx_task(TaskHandle_t task);

/* Configures PE8 (AF7/UART7_TX) push-pull + pull-up + high-speed, 8N1 +
   57600 (S.Port's fixed baud), both invert bits, single-wire half-duplex
   (HDSEL), RXNE always enabled, UART7 NVIC priority 5 (this project's
   established floor for any ISR running alongside FreeRTOS -- see
   board_sbus_uart_init()'s own comment). Call once from the S.Port
   module's start function, after board_sport_uart_set_rx_task(). */
void board_sport_uart_init(void);

/* True if at least one received byte is waiting in the RX ring buffer. */
bool board_sport_uart_available(void);

/* Pops and returns the oldest waiting RX byte. Only valid after
   board_sport_uart_available() returned true. */
uint8_t board_sport_uart_read_byte(void);

/* Queues `length` bytes for transmission and switches the line to drive
   mode -- non-blocking, returns immediately; the actual byte-by-byte
   shifting happens in UART7's own ISR. S.Port frames are at most
   SPORT_MAX_STUFFED_BYTES (see lib/telemetry/sport.c) bytes, comfortably
   inside this driver's ring buffer. */
void board_sport_uart_write(const uint8_t *data, uint8_t length);

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

/* Onboard ICM42688P IMU -- SPI1, CS=PC15 (software-controlled GPIO, not
   SPI1's own hardware NSS), SCK=PA5, MISO=PA6, MOSI=PD7 -- confirmed
   against this project's sibling aoa-boat-controller's
   include/pins_h743.h (itself cross-checked against ArduPilot's
   MatekH743/hwdef.dat and Betaflight's MTKS/MATEKH743 config, plus this
   exact physical unit's own live Betaflight `status`/`resource` output,
   see that file's own top-of-file note) -- issue #26.

   Raw byte-level SPI transaction primitives only, no register map/config
   sequence here -- lib/sensors/icm42688p.c owns that, same board-owns-
   the-bus/lib-owns-the-protocol split board_sport_uart_*() already
   established for UART7 (see that comment above). Keeping the chip
   driver pin-agnostic this way means the same icm42688p.c can be reused
   unchanged on any other board wiring an ICM42688P differently -- e.g.
   nexus_xr, which .docs/hardware.md already lists as carrying the same
   chip family, once that board is unblocked. */
void board_imu_spi_init(void);

/* Writes one register (7-bit address; the chip's own read/write bit is
   applied by icm42688p.c, not here). */
void board_imu_spi_write_reg(uint8_t reg, uint8_t value);

/* Reads `len` bytes starting at register `startReg` into `buf`, in one
   burst SPI transaction. */
void board_imu_spi_read_regs(uint8_t startReg, uint8_t *buf, uint8_t len);

/* Onboard I2C2 -- SCL=PB10, SDA=PB11. Confirmed against ArduPilot's
   MatekH743/hwdef.dat and Betaflight's MTKS/MATEKH743 config (both
   independently agree on PB10/PB11 for this board's I2C2), cross-checked
   against this project's sibling aoa-boat-controller's
   include/pins_h743.h (issue #23). This board's onboard DPS310 baro
   lives here -- a different bus from the SPI1 IMU above, unlike
   afroflight32 where IMU and baro share one I2C bus. Bus-level
   primitives, not baro-specific: I2C1 (PB6/PB7, this board's external-
   compass bus, not yet used by anything) is a separate peripheral
   entirely, so there's no naming collision in leaving these generic for
   whatever else ends up on I2C2 specifically.
   board_i2c2_init() is safe to call more than once (idempotent). */
void board_i2c2_init(void);

/* Returns true on ack, false on a bus/communication failure (NACK,
   timeout, arbitration loss). */
bool board_i2c2_write_reg(uint8_t devAddr, uint8_t reg, uint8_t value);
bool board_i2c2_read_regs(uint8_t devAddr, uint8_t reg, uint8_t *buf, uint8_t len);

/* Battery voltage/current sense -- ADC1, PC0 (VBAT) + PC1 (CURR), this
   board's onboard PDB (power distribution board) feature, not a chip on
   a bus -- issue #24's own body has the full provenance: both pins
   cross-checked against ArduPilot's MatekH743 hwdef.dat
   (BATT_VOLTAGE_SENS/BATT_CURRENT_SENS) and Betaflight's unified-target
   config (ADC_BATT/ADC_CURR) for this exact board, agreeing on PC0/PC1,
   ADC1. Raw 12-bit ADC counts only here -- board owns the peripheral,
   lib/sensors/battery.c owns the divider-scale/amps-per-volt math, same
   board-owns-the-bus/lib-owns-the-protocol split board_imu_spi_*()
   already established.

   ADC clock kept deliberately slow/conservative (per_ck, defaulting to
   HSI ~64MHz since nothing in system_clock_config() touches CKPERSEL or
   enables PLL2, divided by 16 to ~4MHz) for a safe first bring-up --
   comfortably under every H7 boost-mode threshold in RM0433, so HAL's
   own automatic boost configuration (ADC_ConfigureBoostMode, called
   internally by HAL_ADC_Init) stays disabled. NOT bench-verified against
   a real calibrated voltage source yet -- treat readings as unconfirmed
   until checked against a known battery/PSU voltage on the bench, same
   "bench-confirm before trusting" discipline board_imu_spi_init()'s own
   comment already applies to its SPI clock pick. */
void board_battery_adc_init(void);
uint16_t board_battery_adc_read_vbat_raw(void);
uint16_t board_battery_adc_read_curr_raw(void);

/* GPS UART -- issue #56's port/protocol/source model (see
   .docs/architecture/ports.md), replacing #40's original single-port
   design. NMEA input at its standard 115200 8N1, RX only, same
   "receiver only ever transmits to the FC" reasoning
   board_sbus_uart_init() uses for its own TX pin.

   board_gps_uart_init() now takes a port index (params.h's 0-based
   letter encoding, A=0) rather than being hardwired to one UART --
   lib/sensors/gps.c resolves gps.port/gps.source (#54) at gps_start()
   time and passes whatever it resolves to here. Only two of this
   board's ports are actually wired up: Port B (index 1, UART2, PD5/PD6,
   Matek's own "GPS1" suggested use) and Port C (index 2, UART3, PD8/PD9,
   "GPS2" -- the only port #40's original design supported). Returns
   false, initializing nothing, for any other port index -- gps.c treats
   that identically to "no module plugged in", never a crash (see that
   file's own comment on why an unsupported port choice must degrade
   gracefully, not Error_Handler()). RX-only on both: PD8 (USART3_TX) /
   PD5 (USART2_TX) stay unconfigured, confirmed free against every other
   UART/bus this board's board.c already claims (USART6 SBUS/PC7, UART7
   S.Port/PE8, SPI1 IMU, I2C2 baro) -- neither pin pair collides.

   Plain RXNE ISR into one shared ring buffer per active port, same
   lightweight register-level pattern board_sport_uart_*() already
   established for UART7 -- no DMA needed at NMEA's realistic sentence
   rate (a few Hz), unlike SBUS's tight continuous 100000 baud stream.
   Only one of USART2_IRQHandler()/USART3_IRQHandler() (board.c) is ever
   actually enabled at a time -- board_gps_uart_init() only arms RXNE on
   whichever port it just configured, so both handlers safely share the
   same buffer with no risk of concurrent writers.

   This module is a genuine hot-pluggable peripheral, not an onboard
   sensor -- the GPS module needs external power the user connects on
   demand (to avoid draining the boat's main battery), so it can be
   absent at boot or connected mid-session, same as before #56. Absence
   (or an unassigned/unsupported port) just means
   board_gps_uart_available() never returns true, and lib/sensors/gps.c's
   own "last updated" staleness tracking (same never-fabricate-a-value
   convention baro.h/sport.c already use) never advances -- #56's own
   scope note: "unassigned port and unplugged module collapse into the
   same case". See board_gps_uart_init()'s own comment in board.c for
   the fuller provenance. */
bool board_gps_uart_init(uint8_t portIndex);
bool board_gps_uart_available(void);
uint8_t board_gps_uart_read_byte(void);

#endif /* HELM_BOARD_MATEK_H743_H */
