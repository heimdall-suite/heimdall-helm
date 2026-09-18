#ifndef HELM_BOARD_FEATURES_H
#define HELM_BOARD_FEATURES_H

/* All TODO -- no board on the bench yet, don't guess. See board.h. */
#define HELM_HAS_IMU 0
#define HELM_HAS_BARO 0
#define HELM_HAS_MAG 0
#define HELM_HAS_GPS 0
#define HELM_HAS_BLACKBOX_STORAGE 0

#define HELM_FEATURE_BLACKBOX 0
#define HELM_FEATURE_TELEMETRY_SPORT 0
#define HELM_FEATURE_PARAMS_PERSIST 0

/* No ROM-bootloader-DFU or USB-CDC-CLI port exists for STM32F722 either
   -- see afroflight32/board_features.h's own comment. */
#define HELM_HAS_ROM_BOOTLOADER_DFU 0
#define HELM_FEATURE_CLI 0

/* No board_led_toggle() implementation exists for this board yet -- no
   hardware on the bench at all, don't guess (see board.h). */
#define HELM_HAS_DEBUG_LED 0

#endif /* HELM_BOARD_FEATURES_H */
