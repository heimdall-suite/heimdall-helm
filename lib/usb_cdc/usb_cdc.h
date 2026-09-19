#ifndef HELM_USB_CDC_H
#define HELM_USB_CDC_H

#include <stdint.h>

/* Minimal serial console transport, one implementation per board/chip:
   stm32h7.c (matek_h743's native OTG_FS USB CDC device, wiring ST's USB
   Device CDC middleware to that chip's HAL_PCD driver -- see
   boards/matek_h743/usbd_conf.c/usbd_desc.c for the board-specific half)
   and stm32f1.c (afroflight32, #12) -- NOT native USB at all on that
   board: its "USB" port is an onboard USB-serial converter chip wired to
   a plain UART (USART1), confirmed against aoa-boat-controller's real
   firmware for this exact physical board -- see that file's own header
   comment. Despite this header's name, its job is exactly to hide that
   difference: both present the same byte-stream interface to lib/cli
   regardless of what's actually driving the host-visible "USB" port.
   See lib/README.md's chip-not-board convention and platformio.ini's
   custom_helm_usb_cdc/scripts/add_usb_cdc.py for how a board opts in. */

/* Brings up the USB device stack and starts CDC enumeration. Call once,
   after HAL_Init()/board_init() (needs SysTick running for HAL_Delay/
   HAL_GetTick). */
void usb_cdc_init(void);

/* Non-blocking: copies up to maxLen bytes already received from the host
   into buf, returns how many. 0 if nothing is available -- callers
   should poll (e.g. from a task loop with a short vTaskDelay), not spin. */
uint32_t usb_cdc_read(uint8_t *buf, uint32_t maxLen);

/* Queues len bytes for transmission to the host. */
void usb_cdc_write(const uint8_t *buf, uint32_t len);

#endif /* HELM_USB_CDC_H */
