#ifndef HELM_BOARD_AFROFLIGHT32_USBD_CONF_H
#define HELM_BOARD_AFROFLIGHT32_USBD_CONF_H

/* USB Device low-level config for this board -- trimmed from ST's real
   usbd_conf_template.h (Middlewares/ST/STM32_USB_Device_Library/Core/Inc/
   usbd_conf_template.h) to just what the CDC class needs. This is board
   config (which USB peripheral, which GPIOs) rather than a chip driver,
   so it lives here rather than in lib/usb_cdc/ -- see that lib's own
   header for the split. */

#include "stm32f1xx_hal.h"
#include <string.h>

#define USBD_MAX_NUM_INTERFACES 1U
#define USBD_MAX_NUM_CONFIGURATION 1U
/* 128, not matek_h743's 64: this board's own product string ("heimdall-
   helm CLI (afroflight32)", see usbd_desc.c) needs strlen*2+2 = 66 bytes
   once UTF-16LE-encoded -- USBD_GetString() (ST's own usbd_ctlreq.c)
   writes that many bytes into strDescScratch with NO bounds check
   against this constant, so leaving it at 64 would silently overflow
   the buffer by 2 bytes. Sized with real headroom instead of the
   minimum, so a longer string later doesn't reopen the same bug. */
#define USBD_MAX_STR_DESC_SIZ 128U
#define USBD_SELF_POWERED 0U
#define USBD_DEBUG_LEVEL 0U /* no printf retargeted here -- keep the USBD_*Log macros as no-ops */

#define USBD_CDC_INTERVAL 2000U

#define USBD_malloc (void *)USBD_static_malloc
#define USBD_free USBD_static_free
#define USBD_memset memset
#define USBD_memcpy memcpy
#define USBD_Delay HAL_Delay

#define USBD_UsrLog(...) \
    do {                 \
    } while (0)
#define USBD_ErrLog(...) \
    do {                 \
    } while (0)
#define USBD_DbgLog(...) \
    do {                 \
    } while (0)

void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#endif /* HELM_BOARD_AFROFLIGHT32_USBD_CONF_H */
