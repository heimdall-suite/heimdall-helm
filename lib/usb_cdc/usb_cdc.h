#ifndef HELM_USB_CDC_H
#define HELM_USB_CDC_H

#include <stdint.h>

/* Minimal USB CDC-ACM (virtual COM port) transport -- one implementation
   per chip with a USB device peripheral (currently just stm32h7.c, which
   wires ST's USB Device CDC middleware to this board's OTG_FS; see
   boards/matek_h743/usbd_conf.c/usbd_desc.c for the board-specific half
   of that wiring). See lib/README.md's chip-not-board convention and
   platformio.ini's custom_helm_usb_cdc/scripts/add_usb_cdc.py for how a
   board opts in. This is the transport lib/cli runs over (via lib/shell,
   which itself has no USB dependency -- see its own header). */

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
