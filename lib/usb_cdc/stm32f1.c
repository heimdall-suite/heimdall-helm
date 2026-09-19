#include "usb_cdc.h"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"

/* CDC-ACM transport over STM32F1's USB Device middleware -- issue #12.
   Same role as stm32h7.c (owns the USBD_HandleTypeDef and the CDC class
   fops ST's usbd_cdc_if_template.c leaves for the integrator to fill
   in), but not a copy-paste of it: framework-stm32cubef1 ships an older
   revision of ST's CDC class (Middlewares/ST/STM32_USB_Device_Library/
   Class/CDC) whose USBD_CDC_ItfTypeDef has no TransmitCplt callback --
   confirmed by diffing both frameworks' usbd_cdc.h before writing this,
   not assumed. Not a problem: usb_cdc_write() below never relied on that
   callback firing, it already polls the class handle's TxState directly,
   same as stm32h7.c does. The board-specific half (GPIO/clock/NVIC,
   PMA endpoint layout, descriptors) lives in boards/afroflight32/
   instead of here -- see that board's usbd_conf.c for the real-source
   derivation (ST's STM3210E_EVAL CDC_Standalone example). */

#define RX_RING_SIZE 256U

static uint8_t rxRing[RX_RING_SIZE];
static volatile uint32_t rxHead = 0; /* next slot the ISR writes */
static volatile uint32_t rxTail = 0; /* next slot the reader takes */

static USBD_HandleTypeDef hUsbDeviceFS;
static uint8_t cdcRxBuffer[CDC_DATA_FS_MAX_PACKET_SIZE];

static int8_t cdc_init(void);
static int8_t cdc_deinit(void);
static int8_t cdc_control(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t cdc_receive(uint8_t *buf, uint32_t *len);

static USBD_CDC_ItfTypeDef cdcFops = {
    cdc_init,
    cdc_deinit,
    cdc_control,
    cdc_receive,
};

/* ST CDC class callback: called once the class is registered. Hands the
   class our fixed receive buffer to fill on each incoming packet. */
static int8_t cdc_init(void) {
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, NULL, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, cdcRxBuffer);
    return (int8_t)USBD_OK;
}

/* ST CDC class callback: called on class deinit. Nothing to release --
   cdcRxBuffer is static, not allocated. */
static int8_t cdc_deinit(void) {
    return (int8_t)USBD_OK;
}

/* ST CDC class callback: handles CDC control requests (SET_LINE_CODING
   etc.). */
static int8_t cdc_control(uint8_t cmd, uint8_t *pbuf, uint16_t length) {
    (void)cmd;
    (void)pbuf;
    (void)length;
    /* No line-coding/control-line state to track for this CLI -- the
       host's terminal settings don't change how we frame bytes. */
    return (int8_t)USBD_OK;
}

/* ST CDC class callback: called with each received packet. Copies it into
   our own ring buffer so usb_cdc_read() can drain it outside IRQ context,
   then re-arms the endpoint for the next packet. */
static int8_t cdc_receive(uint8_t *buf, uint32_t *len) {
    /* Runs in USB IRQ context. Drop bytes on ring overflow rather than
       block -- a CLI console losing a byte under overflow is fine, an
       ISR blocking is not. */
    for (uint32_t i = 0; i < *len; i++) {
        uint32_t const next = (rxHead + 1U) % RX_RING_SIZE;
        if (next != rxTail) {
            rxRing[rxHead] = buf[i];
            rxHead = next;
        }
    }

    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (int8_t)USBD_OK;
}

void usb_cdc_init(void) {
    USBD_Init(&hUsbDeviceFS, &HELM_USBD_Desc, 0);
    USBD_RegisterClass(&hUsbDeviceFS, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &cdcFops);
    USBD_Start(&hUsbDeviceFS);

    /* No separate USB supply-rail voltage detector to enable here --
       that's an H7-specific quirk (see stm32h7.c's own comment), not a
       thing on F103's USB peripheral. */
}

uint32_t usb_cdc_read(uint8_t *buf, uint32_t maxLen) {
    uint32_t count = 0;
    while (count < maxLen && rxTail != rxHead) {
        buf[count] = rxRing[rxTail];
        rxTail = (rxTail + 1U) % RX_RING_SIZE;
        count++;
    }
    return count;
}

void usb_cdc_write(const uint8_t *buf, uint32_t len) {
    USBD_CDC_HandleTypeDef *cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (cdc == NULL) {
        return; /* not enumerated yet -- nothing to write to */
    }

    uint32_t sent = 0;
    while (sent < len) {
        uint32_t const chunk = (len - sent) > CDC_DATA_FS_MAX_PACKET_SIZE ? CDC_DATA_FS_MAX_PACKET_SIZE
                                                                           : (len - sent);

        /* Wait for the previous chunk to actually go out -- this
           middleware revision has no TransmitCplt callback to wait on
           instead (see this file's header comment), so polling TxState
           directly is the only option here, not just the simpler one.
           Bounded spin, not a real timeout: acceptable for a debug
           console that only ever sends short lines, not for a link this
           project depends on for control-relevant data. */
        uint32_t guard = 1000000U;
        while (cdc->TxState != 0U && guard > 0U) {
            guard--;
        }

        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)(buf + sent), (uint16_t)chunk);
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);
        sent += chunk;
    }
}
