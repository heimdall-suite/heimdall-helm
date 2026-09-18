#include "usb_cdc.h"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"

/* CDC-ACM transport over STM32H7's USB Device middleware (OTG_FS). Owns
   the USBD_HandleTypeDef and the CDC class fops (Init/DeInit/Control/
   Receive/TransmitCplt) that ST's usbd_cdc_if_template.c
   (Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc/
   usbd_cdc_if_template.h) leaves for the integrator to fill in -- see
   boards/matek_h743/usbd_conf.c's own header comment for why this is
   hand-written rather than CubeMX-generated. The board-specific half
   (GPIO/clock/NVIC, descriptors) lives in boards/matek_h743/ instead of
   here -- this file only owns the parts that are the same for any board
   carrying this same USB peripheral. */

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
static int8_t cdc_transmit_complete(uint8_t *buf, uint32_t *len, uint8_t epnum);

static USBD_CDC_ItfTypeDef cdcFops = {
    cdc_init,
    cdc_deinit,
    cdc_control,
    cdc_receive,
    cdc_transmit_complete,
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

/* ST CDC class callback: called when a queued TX packet finishes sending.
   Nothing to do -- usb_cdc_write() polls cdc->TxState itself rather than
   waiting on this callback. */
static int8_t cdc_transmit_complete(uint8_t *buf, uint32_t *len, uint8_t epnum) {
    (void)buf;
    (void)len;
    (void)epnum;
    return (int8_t)USBD_OK;
}

void usb_cdc_init(void) {
    USBD_Init(&hUsbDeviceFS, &HELM_USBD_Desc, 0);
    USBD_RegisterClass(&hUsbDeviceFS, USBD_CDC_CLASS);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &cdcFops);
    USBD_Start(&hUsbDeviceFS);

    /* H7's USB transceivers run off a separate VDD33USB rail with its own
       level detector (PWR_CR3.USB33DEN) -- without enabling it, every
       digital HAL/USBD call above still returns HAL_OK, but the analog
       FS PHY never actually drives D+/D-, so the device is invisible to
       the host despite firmware behaving as if everything worked
       (bench-confirmed: exactly this symptom, diagnosed via a heartbeat
       LED since no debug probe is available -- see boards/matek_h743/
       usbd_conf.c's header comment). Ported from Betaflight's real,
       deployed src/platform/STM32/serial_usb_vcp.c, which calls this
       after USBD_Start() specifically on STM32H7 with the same delay and
       comment ("Cold boot failures observed without this, even when USB
       cable is not connected") -- not derived here. */
    HAL_PWREx_EnableUSBVoltageDetector();
    HAL_Delay(100);
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

        /* Wait for the previous chunk's TransmitCplt -- USBD_CDC_SetTxBuffer/
           TransmitPacket must not be called again while TxState is busy.
           Bounded spin, not a real timeout: acceptable for a debug console
           that only ever sends short lines, not for a link this project
           depends on for control-relevant data. */
        uint32_t guard = 1000000U;
        while (cdc->TxState != 0U && guard > 0U) {
            guard--;
        }

        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)(buf + sent), chunk);
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);
        sent += chunk;
    }
}
