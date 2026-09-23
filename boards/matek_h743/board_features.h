#ifndef HELM_BOARD_FEATURES_H
#define HELM_BOARD_FEATURES_H

/* HELM_HAS_*: hardware facts -- is this chip physically wired on this
   board. Carried over from aoa-boat-controller's H743 bench unit (same
   physical hardware); re-verify against that project's current
   docs/build-log.md before trusting, these were last cross-checked
   2026-09-17 and that project's hardware bring-up was still active. */
#define HELM_HAS_IMU 1   /* ICM42688P, SPI -- confirmed */
#define HELM_HAS_BARO 1  /* DPS310, I2C -- confirmed present in code, verify wiring */
#define HELM_HAS_MAG 0   /* not wired, confirmed absent as of last check */
#define HELM_HAS_BLACKBOX_STORAGE 0 /* no SD/flash wired for logging on this unit -- TODO */
#define HELM_HAS_BATTERY_SENSE 1 /* onboard PDB (power distribution board), ADC1
                                     PC0 (VBAT) + PC1 (CURR) -- confirmed against
                                     ArduPilot/Betaflight targets for this exact
                                     board, see issue #24's own body */

/* HELM_FEATURE_*: software capability toggles. Nothing is implemented yet
   (see repo root README Status) -- these declare INTENT for the module
   architecture, not current capability. This board is a "full" target
   (most RAM/flash headroom of the three), so nothing is deliberately cut
   here the way afroflight32.h cuts things for resource reasons. */
#define HELM_FEATURE_BLACKBOX 1
/* HELM_FEATURE_TELEMETRY: the protocol-agnostic gather task + table
   (issue #16) -- separate from which protocol adapter(s) actually put
   it on a wire (HELM_FEATURE_TELEMETRY_SPORT below, issue #18; a future
   _CRSF, issue #19). See .docs/architecture/telemetry.md. */
#define HELM_FEATURE_TELEMETRY 1
#define HELM_FEATURE_TELEMETRY_SPORT 1
#define HELM_FEATURE_PARAMS_PERSIST 1

/* HELM_HAS_ROM_BOOTLOADER_JUMP: this chip's ROM DFU-jump technique (RTC
   backup register + reset, see lib/bootloader/stm32h7.c) is implemented
   and bench-confirmed working on this exact physical board (see that
   file's own header comment) -- 1 here, 0 on boards without a ported
   implementation. */
#define HELM_HAS_ROM_BOOTLOADER_JUMP 1

/* HELM_FEATURE_CLI: build the USB-CDC CLI console (lib/shell + lib/cli +
   lib/usb_cdc) and register its bootloader-DFU command. Off on boards
   with no USB CDC transport ported yet. */
#define HELM_FEATURE_CLI 1

/* HELM_HAS_DEBUG_LED: board_led_toggle() (board.h) exists and its pin/
   polarity is bench-confirmed on this board -- see that function's own
   comment. Gates lib/debug/heartbeat.c's LED toggle. */
#define HELM_HAS_DEBUG_LED 1

/* HELM_HAS_IWDG: board_iwdg_init()/board_iwdg_refresh() (board.h) are
   implemented for this chip family (STM32H7 IWDG1, LSI-clocked) --
   issue #5. Gates lib/supervisor/supervisor.c's watchdog start/feed;
   0 on boards without a ported IWDG driver yet (afroflight32/nexus_xr --
   register-level per chip family, same standard as every other
   clock/peripheral decision here, ported separately per the issue). */
#define HELM_HAS_IWDG 1

/* HELM_RX_DEFAULT_PROTOCOL_*: compile-time fallback for which lib/rx/
   driver rx_start() binds, used until HELM_FEATURE_PARAMS_PERSIST's
   persisted input-mode param exists (#10) -- see .docs/architecture/
   module-architecture.md's RX case study. This board's receiver wiring
   is SBUS. Exactly one of these must be 1. */
#define HELM_RX_DEFAULT_PROTOCOL_SBUS 1
#define HELM_RX_DEFAULT_PROTOCOL_CRSF 0

/* HELM_HAS_PORT_<X>_UART / HELM_HAS_PORT_<X>_I2C: per-port hardware-
   capability facts (issue #53), one pair per port letter established by
   #52's sourced audit (.docs/hardware.md's port inventory /
   .docs/architecture/ports.md) -- does this port physically exist and
   what transport(s) can it carry, nothing about what's plugged into it or
   what role currently claims it. A different, more general kind of fact
   than HELM_HAS_SBUS_UART/HELM_HAS_SPORT_UART below, which track a
   specific *role* being wired and bench-confirmed on a specific port --
   both categories coexist deliberately, see ports.md for why HELM_HAS_GPS's
   old design conflated them. All 9 of this board's ports (7 UART + 2 I2C,
   letters A-I) are sourced and confirmed present -- see hardware.md's own
   table for each port's pins/silk label. */
#define HELM_HAS_PORT_A_UART 1 /* UART1, PA9/PA10, silk `TX1 RX1` */
#define HELM_HAS_PORT_B_UART 1 /* UART2, PD5/PD6, silk `TX2 RX2` */
#define HELM_HAS_PORT_C_UART 1 /* UART3, PD8/PD9, silk `TX3 RX3` -- gps.port's
                                   default (#56), one of the two ports
                                   board_gps_uart_init() can bind GPS to */
#define HELM_HAS_PORT_D_UART 1 /* UART4, PB9/PB8, silk `TX4 RX4` */
#define HELM_HAS_PORT_E_UART 1 /* UART6, PC6/PC7, silk `TX6 RX6` -- where
                                   HELM_HAS_SBUS_UART below is wired today (#8) */
#define HELM_HAS_PORT_F_UART 1 /* UART7, PE7/PE8, silk `RX7 TX7` -- where
                                   HELM_HAS_SPORT_UART below is wired today (#18) */
#define HELM_HAS_PORT_G_UART 1 /* UART8, PE1/PE0, silk `TX8 RX8` */
#define HELM_HAS_PORT_H_I2C 1  /* I2C1, PB6/PB7, silk `CL1 DA1` */
#define HELM_HAS_PORT_I_I2C 1  /* I2C2, PB10/PB11, silk `CL2 DA2` -- onboard,
                                   carries HELM_HAS_BARO's DPS310 */

/* HELM_HAS_SBUS_UART: this board's real SBUS UART wiring
   (board_sbus_uart_init()/board_sbus_uart_take_frame() in board.c/board.h)
   is implemented and bench-confirmed -- USART6/PC7 ("RX6" silk), see
   board.h's own comment for the full provenance (issue #8). Gates
   lib/rx/sbus.c's real decode path; 0 on boards without a confirmed SBUS
   UART wiring yet (falls back to sbus.c's fixed-test-data stub instead,
   same as every driver was before #8). */
#define HELM_HAS_SBUS_UART 1

/* HELM_HAS_SPORT_UART: this board's real S.Port UART wiring
   (board_sport_uart_* in board.c/board.h) is implemented and wired to a
   real receiver -- UART7/PE8, silk-labeled "TX7", ported from
   aoa-boat-controller's own bench-verified SportUart (issue #18). Gates
   lib/telemetry/sport.c's entire body (see that file's own comment);
   0 on boards without a ported S.Port UART transport, same category as
   HELM_HAS_SBUS_UART above. Independent of HELM_FEATURE_TELEMETRY_SPORT
   (board "wants" S.Port telemetry) the same way HELM_HAS_ROM_BOOTLOADER_JUMP
   is independent of HELM_FEATURE_CLI -- see .docs/cli.md's own comment
   on why these stay two separate flags. */
#define HELM_HAS_SPORT_UART 1

/* HELM_HAS_GPS_UART_TRANSPORT: this board's board_gps_uart_*() (board.h/
   board.c) is implemented and can genuinely bind to more than one port
   at runtime -- Port B (UART2, GPS1) or Port C (UART3, GPS2), matching
   Matek's own suggested-use labels (.docs/hardware.md). Issue #56's own
   replacement for the retired HELM_HAS_GPS flag, which wrongly
   conflated "a UART is wired to a GPS header" with "role = gps" and
   "only one possible port" -- see ports.md. Gates lib/sensors/gps.c's
   whole-file compile (same idiom HELM_HAS_SBUS_UART/HELM_HAS_SPORT_UART
   above already use); which port/protocol/source is actually active is
   entirely a runtime pick now (gps.port/gps.protocol/gps.source, #54),
   never baked into this flag. 0 on boards without board_gps_uart_*()
   implemented at all, same category as the two flags above. */
#define HELM_HAS_GPS_UART_TRANSPORT 1

/* HELM_SERVO_COUNT: real per-board hardware fact (issue #36) -- how many
   physical servo connectors output.c's slotConfigs[] table sizes itself
   to, generally fewer than RX_MAX_CHANNELS (rx.h). Count only, not real
   pin/timer facts yet -- those are issue #31's job (real PWM driver),
   still open; #36 only needs to know how many slots exist. 8 (S3-S10)
   per issue #31's own body, itself sourced from aoa-boat-controller's
   pin headers -- re-verify against physical hardware before trusting,
   same as every other pin/count fact carried over from that project. */
#define HELM_SERVO_COUNT 8

#endif /* HELM_BOARD_FEATURES_H */
