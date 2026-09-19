#ifndef HELM_BOARD_FEATURES_H
#define HELM_BOARD_FEATURES_H

/* All TODO -- no board on the bench yet, don't guess. See board.h. */
#define HELM_HAS_IMU 0
#define HELM_HAS_BARO 0
#define HELM_HAS_MAG 0
#define HELM_HAS_GPS 0
#define HELM_HAS_BLACKBOX_STORAGE 0

#define HELM_FEATURE_BLACKBOX 0
/* HELM_FEATURE_TELEMETRY: the protocol-agnostic gather task + table
   (issue #16), separate from HELM_FEATURE_TELEMETRY_SPORT below -- see
   matek_h743/board_features.h's own comment. 0 here for the same reason
   everything else on this board is: no hardware on the bench, board.c
   still #errors. */
#define HELM_FEATURE_TELEMETRY 0
#define HELM_FEATURE_TELEMETRY_SPORT 0
#define HELM_FEATURE_PARAMS_PERSIST 0

/* No ROM-bootloader-DFU or USB-CDC-CLI port exists for STM32F722 either
   -- see afroflight32/board_features.h's own comment. */
#define HELM_HAS_ROM_BOOTLOADER_DFU 0
#define HELM_FEATURE_CLI 0

/* No board_led_toggle() implementation exists for this board yet -- no
   hardware on the bench at all, don't guess (see board.h). */
#define HELM_HAS_DEBUG_LED 0

/* No IWDG driver ported for STM32F7 yet -- see afroflight32/
   board_features.h's own comment; this board is blocked on its
   hardware #error anyway. */
#define HELM_HAS_IWDG 0

/* HELM_RX_DEFAULT_PROTOCOL_*: compile-time fallback for which lib/rx/
   driver rx_start() binds, used until HELM_FEATURE_PARAMS_PERSIST's
   persisted input-mode param exists (#10) -- see .docs/architecture/
   module-architecture.md's RX case study. The XR's onboard ExpressLRS
   receiver uses CRSF framing, not SBUS. Exactly one of these must be
   1. */
#define HELM_RX_DEFAULT_PROTOCOL_SBUS 0
#define HELM_RX_DEFAULT_PROTOCOL_CRSF 1

/* HELM_HAS_SBUS_UART: no hardware on the bench at all, and this board
   defaults to CRSF anyway -- see afroflight32/board_features.h's own
   comment on what this flag gates. */
#define HELM_HAS_SBUS_UART 0

#endif /* HELM_BOARD_FEATURES_H */
