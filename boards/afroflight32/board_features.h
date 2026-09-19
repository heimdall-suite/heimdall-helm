#ifndef HELM_BOARD_FEATURES_H
#define HELM_BOARD_FEATURES_H

/* HELM_HAS_*: hardware facts -- is this chip physically wired on this
   board. Carried over from aoa-boat-controller's Naze32/Afroflight32 Rev6
   bench unit; re-verify against that project's current docs/build-log.md
   before trusting -- that project's hardware bring-up on this board was
   still active as of the last check (2026-09-17). */
#define HELM_HAS_IMU 1   /* MPU6500, I2C -- confirmed */
#define HELM_HAS_BARO 0  /* bring-up in progress upstream, chip/wiring not confirmed here yet -- TODO */
#define HELM_HAS_MAG 0   /* not confirmed wired -- TODO */
#define HELM_HAS_GPS 0   /* not confirmed wired -- TODO */
#define HELM_HAS_BLACKBOX_STORAGE 1 /* 2MB SPI NOR on SPI2, all 4 pins free -- confirmed */

/* HELM_FEATURE_*: software capability toggles, deliberately reduced on
   this target. This is the resource-constrained board of the three (20KB
   RAM / 128KB flash total -- for comparison, the H743's FreeRTOS heap
   ALONE is configured larger than this board's entire RAM). Nothing is
   implemented yet (see repo root README Status), so these declare INTENT/
   BUDGET, not current capability -- but they're deliberately more
   conservative here than matek_h743.h's, not just "not built yet".

   TODO: these are placeholder decisions, not final -- revisit once real
   modules exist and their actual RAM/flash cost is measured on this
   target specifically. */
#define HELM_FEATURE_BLACKBOX 1        /* storage exists (SPI NOR); cost is the logging
                                           task's RAM/CPU budget, not storage capacity */
#define HELM_FEATURE_TELEMETRY_SPORT 1 /* core to this project's purpose, keep on every target */
#define HELM_FEATURE_PARAMS_PERSIST 0  /* cut for now -- TODO: revisit once a persistence
                                           backend is designed and its footprint is known */

/* No USB-CDC-CLI port exists for STM32F103 yet -- would need its own
   from-real-source derivation and bench verification, same standard as
   every other register-level decision here (see lib/bootloader/
   stm32h7.c's header comment). Not attempted yet, not a RAM/flash budget
   decision like the flags above. This board IS planned to get a CLI
   eventually, unlike HELM_HAS_ROM_BOOTLOADER_DFU below -- the two aren't
   the same port, don't assume one implies the other. */
#define HELM_FEATURE_CLI 0

/* This board won't get the CLI's `dfu` bootloader-jump command even once
   HELM_FEATURE_CLI above lands -- entering this chip's bootloader stays
   a manual (BOOT0-strap) step, unlike matek_h743's software jump (see
   lib/bootloader/stm32h7.c). Separate capability from HELM_FEATURE_CLI,
   not just "not ported yet" the same way that flag is. */
#define HELM_HAS_ROM_BOOTLOADER_DFU 0

/* No board_led_toggle() implementation exists for this board yet -- no
   LED pin bench-confirmed here, same standard as the flags above. */
#define HELM_HAS_DEBUG_LED 0

/* No IWDG driver ported for STM32F1 yet -- issue #5 starts with
   matek_h743 (real bench-derived config) and ports this separately,
   same standard as every other register-level decision here. */
#define HELM_HAS_IWDG 0

/* HELM_RX_DEFAULT_PROTOCOL_*: compile-time fallback for which lib/rx/
   driver rx_start() binds, used until HELM_FEATURE_PARAMS_PERSIST's
   persisted input-mode param exists (#10) -- see .docs/architecture/
   module-architecture.md's RX case study. This board's receiver wiring
   is SBUS. Exactly one of these must be 1. */
#define HELM_RX_DEFAULT_PROTOCOL_SBUS 1
#define HELM_RX_DEFAULT_PROTOCOL_CRSF 0

/* HELM_HAS_SBUS_UART: no confirmed SBUS UART wiring on this board yet --
   see the #8 kickoff notes (which UART/pin isn't decided here, unlike
   matek_h743's bench-confirmed USART6/PC7). Not blocking: lib/rx/sbus.c
   falls back to its fixed-test-data stub when this is 0, same behavior
   the #7 pipeline was already bench-verified against on this board. */
#define HELM_HAS_SBUS_UART 0

#endif /* HELM_BOARD_FEATURES_H */
