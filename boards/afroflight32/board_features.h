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
/* HELM_FEATURE_TELEMETRY: the protocol-agnostic gather task + table
   (issue #16), separate from HELM_FEATURE_TELEMETRY_SPORT below (issue
   #18, which protocol adapter(s) actually put it on a wire) -- see
   .docs/architecture/telemetry.md. One QueueHandle_t per table field, a
   few bytes each; not expected to be the thing that blows this board's
   RAM budget, but worth re-measuring once the table has more than
   issue #17's one placeholder field. */
#define HELM_FEATURE_TELEMETRY 1
#define HELM_FEATURE_TELEMETRY_SPORT 1 /* core to this project's purpose, keep on every target */
#define HELM_FEATURE_PARAMS_PERSIST 0  /* cut for now -- TODO: revisit once a persistence
                                           backend is designed and its footprint is known */

/* CLI transport ported as of #12/#13: lib/usb_cdc/stm32f1.c, NOT native
   USB -- this board's "USB" port is an onboard USB-serial converter chip
   wired to a plain UART (USART1), confirmed against aoa-boat-controller's
   real firmware for this exact physical board (see that file's own
   header comment). Bench-verified on the real unit: `status`/`help`/
   `diag pipeline` all round-trip correctly over it. Getting there also
   required src/main.c's SCB->VTOR fix (this board's bootloader-jump
   entry leaves interrupts vectoring into the ROM bootloader's own stale
   table otherwise) and bumping configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h (6KB measured too small once vTaskStartScheduler()'s
   own IDLE/timer tasks are counted) -- see both files' own comments.
   Separate capability from HELM_HAS_ROM_BOOTLOADER_DFU below -- don't
   assume one implies the other. */
#define HELM_FEATURE_CLI 1

/* This board doesn't get the CLI's `dfu` bootloader-jump command even
   though HELM_FEATURE_CLI above is on -- entering this chip's bootloader
   stays a manual (BOOT0-strap) step, unlike matek_h743's software jump
   (see lib/bootloader/stm32h7.c). Separate capability from
   HELM_FEATURE_CLI, not just "not ported yet" the same way that flag
   was -- lib/cli/cli.c's `dfu` command is gated on this flag specifically
   (issue #13), not on HELM_FEATURE_CLI, so leaving this 0 is enough to
   keep it out of the build without needing lib/bootloader ported here. */
#define HELM_HAS_ROM_BOOTLOADER_DFU 0

/* board_led_toggle() (board.h) drives PB4, the "CAL" LED -- pin/polarity
   confirmed against aoa-boat-controller's real firmware for this exact
   physical board (include/pins_naze32.h's PIN_CAL_LED). This board does
   physically have onboard LEDs; this flag was 0 only because nothing in
   this project had implemented the driver yet, not because the hardware
   was in doubt -- don't conflate "not wired up here yet" with "board has
   none" the way an earlier comment on this line did. */
#define HELM_HAS_DEBUG_LED 1

/* IWDG ported for STM32F1 and bench-confirmed on this exact board --
   issue #11 (boards/afroflight32/board.c's board_iwdg_init()/
   board_iwdg_refresh(), see that file's own comment for the
   diag-wedge verification run). */
#define HELM_HAS_IWDG 1

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

/* HELM_HAS_SPORT_UART: no ported S.Port UART transport on this board --
   see matek_h743/board_features.h's own comment for what this gates.
   TODO if this board ever gets its own S.Port receiver wired: this
   board's free UARTs would need checking against
   aoa-boat-controller's Naze32 target first, not assumed from the
   H743's UART7/PE8 choice. */
#define HELM_HAS_SPORT_UART 0

#endif /* HELM_BOARD_FEATURES_H */
