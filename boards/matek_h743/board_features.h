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
#define HELM_HAS_GPS 0   /* not wired, confirmed absent as of last check */
#define HELM_HAS_BLACKBOX_STORAGE 0 /* no SD/flash wired for logging on this unit -- TODO */

/* HELM_FEATURE_*: software capability toggles. Nothing is implemented yet
   (see repo root README Status) -- these declare INTENT for the module
   architecture, not current capability. This board is a "full" target
   (most RAM/flash headroom of the three), so nothing is deliberately cut
   here the way afroflight32.h cuts things for resource reasons. */
#define HELM_FEATURE_BLACKBOX 1
#define HELM_FEATURE_TELEMETRY_SPORT 1
#define HELM_FEATURE_PARAMS_PERSIST 1

/* HELM_HAS_ROM_BOOTLOADER_DFU: this chip's ROM DFU-jump technique (RTC
   backup register + reset, see lib/bootloader/stm32h7.c) is implemented
   and bench-confirmed working on this exact physical board (see that
   file's own header comment) -- 1 here, 0 on boards without a ported
   implementation. */
#define HELM_HAS_ROM_BOOTLOADER_DFU 1

/* HELM_FEATURE_CLI: build the USB-CDC CLI console (lib/shell + lib/cli +
   lib/usb_cdc) and register its bootloader-DFU command. Off on boards
   with no USB CDC transport ported yet. */
#define HELM_FEATURE_CLI 1

/* HELM_HAS_DEBUG_LED: board_led_toggle() (board.h) exists and its pin/
   polarity is bench-confirmed on this board -- see that function's own
   comment. Gates lib/debug/heartbeat.c's LED toggle. */
#define HELM_HAS_DEBUG_LED 1

#endif /* HELM_BOARD_FEATURES_H */
