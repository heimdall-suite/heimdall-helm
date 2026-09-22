#ifndef HELM_BOARD_FEATURES_H
#define HELM_BOARD_FEATURES_H

/* All TODO -- no board on the bench yet, don't guess. See board.h. */
#define HELM_HAS_IMU 0
#define HELM_HAS_BARO 0
#define HELM_HAS_MAG 0
#define HELM_HAS_GPS 0
#define HELM_HAS_BLACKBOX_STORAGE 0
#define HELM_HAS_BATTERY_SENSE 0

#define HELM_FEATURE_BLACKBOX 0
/* HELM_FEATURE_TELEMETRY: the protocol-agnostic gather task + table
   (issue #16), separate from HELM_FEATURE_TELEMETRY_SPORT below -- see
   matek_h743/board_features.h's own comment. 0 here for the same reason
   everything else on this board is: no hardware on the bench, board.c
   still #errors. */
#define HELM_FEATURE_TELEMETRY 0
#define HELM_FEATURE_TELEMETRY_SPORT 0
#define HELM_FEATURE_PARAMS_PERSIST 0

/* No ROM-bootloader-jump or USB-CDC-CLI port exists for STM32F722 either
   -- see afroflight32/board_features.h's own comment. */
#define HELM_HAS_ROM_BOOTLOADER_JUMP 0
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

/* HELM_HAS_PORT_<X>_UART / HELM_HAS_PORT_<X>_I2C: per-port hardware-
   capability facts (issue #53), same category as matek_h743's own block of
   these -- see that file's comment for the full rationale. Sourced from
   INAV's own NEXUSX pinout doc, same as #52's audit (.docs/hardware.md) --
   but held at 0 here anyway, same policy as every other flag in this file:
   no board on the bench yet, don't trust a documented pinout over real
   hardware for a target that still `#error`s in board.c. Flip these once
   real hardware exists to bench-confirm against, not before. */
#define HELM_HAS_PORT_A_UART 0 /* UART4 */
#define HELM_HAS_PORT_B_UART 0 /* UART6 */
#define HELM_HAS_PORT_C_UART 0 /* UART3, alternate-function with I2C2 below (#52) */
#define HELM_HAS_PORT_C_I2C 0  /* I2C2, alternate-function with UART3 above --
                                   genuinely exclusive, needs a `.mode` field (#54) */
#define HELM_HAS_PORT_D_UART 0 /* UART1, AUX/SBUS header pins, 3-way
                                   alternate-function (servo/UART1/I2C1, #52).
                                   Only 2 of the 3 states are flagged here --
                                   might add `HELM_HAS_PORT_D_PWM` in the
                                   future to support servo output, once the
                                   output-mapping system and this port's claim
                                   state have a way to arbitrate which one
                                   wins (still an open design question, see
                                   hardware.md's "Open items") */
#define HELM_HAS_PORT_D_I2C 0  /* I2C1, same AUX/SBUS header pins as above --
                                   same 3-way alternate-function, see the PWM
                                   note above */
#define HELM_HAS_PORT_E_UART 0 /* UART5, onboard, wired to the built-in
                                   dual-SX1281 ExpressLRS receiver -- not
                                   exposed to any connector, not a claimable
                                   port (ports.md's `source = onboard` case) */

/* HELM_HAS_SBUS_UART: no hardware on the bench at all, and this board
   defaults to CRSF anyway -- see afroflight32/board_features.h's own
   comment on what this flag gates. */
#define HELM_HAS_SBUS_UART 0

/* HELM_HAS_SPORT_UART: no hardware on the bench at all -- see
   matek_h743/board_features.h's own comment for what this gates. */
#define HELM_HAS_SPORT_UART 0

/* HELM_SERVO_COUNT: no board on the bench yet, don't guess -- same as
   every other flag in this file (see board.h). */
#define HELM_SERVO_COUNT 0

#endif /* HELM_BOARD_FEATURES_H */
