#ifndef HELM_BOARD_AFROFLIGHT32_H
#define HELM_BOARD_AFROFLIGHT32_H

#define HELM_BOARD_NAME "afroflight32"

/* Called once from main.c, after HAL_Init(), before the scheduler starts.
   Brings up this board's clock tree and any other early, board-specific
   peripheral init that has to happen before modules/drivers touch
   hardware. */
void board_init(void);

#endif /* HELM_BOARD_AFROFLIGHT32_H */
