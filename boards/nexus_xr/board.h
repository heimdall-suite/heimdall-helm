#ifndef HELM_BOARD_NEXUS_XR_H
#define HELM_BOARD_NEXUS_XR_H

#define HELM_BOARD_NAME "nexus_xr"

/* STM32F722RET6, ICM42688P IMU, SPL06-001 baro, W25N02KVZEIR blackbox
   flash, 9-pin servo header, 3x independent UART (Port A/B/C), XR variant
   adds an onboard dual-SX1281 ExpressLRS receiver on its own UART5. See
   aoa-boat-controller's docs/nexus-xr-exploration.md for the full
   writeup -- explicitly speculative research, never bench-verified.

   NOT YET BUILDABLE: no board on the bench, no confirmed HSE crystal
   value, no confirmed pin map beyond the spec-sheet-inferred servo/UART
   layout in that doc. board.c intentionally #errors -- do not fill in
   guessed values here, confirm against real hardware or an actual
   schematic first. */
void board_init(void);

#endif /* HELM_BOARD_NEXUS_XR_H */
