#ifndef HELM_BOARD_MATEK_H743_H
#define HELM_BOARD_MATEK_H743_H

#define HELM_BOARD_NAME "matek_h743"

/* Called once from main.c, after HAL_Init(), before the scheduler starts.
   Brings up this board's clock tree and any other early, board-specific
   peripheral init that has to happen before modules/drivers touch
   hardware. */
void board_init(void);

#endif /* HELM_BOARD_MATEK_H743_H */
