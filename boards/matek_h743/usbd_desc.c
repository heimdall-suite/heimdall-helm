#include "usbd_desc.h"
#include "usbd_core.h"
#include "usbd_conf.h"

/* USB descriptors for this board -- trimmed from ST's real
   usbd_desc_template.c, filled in with real values instead of the
   template's placeholder "xxxxx"/0xaaaa strings. VID:PID (0x0483:0x5740)
   is ST's own published demo pair for a CDC Virtual COM Port (not
   invented here), deliberately different from the ROM bootloader's
   0483:df11 so host tooling/OS can tell "running app, CLI available" and
   "sitting in DFU, ready to flash" apart on sight. */

#define USBD_VID 0x0483U
#define USBD_PID 0x5740U
#define USBD_LANGID_STRING 0x0409U /* en-US */
#define USBD_MANUFACTURER_STRING "heimdall-suite"
#define USBD_PRODUCT_FS_STRING "heimdall-helm CLI (matek_h743)"
#define USBD_CONFIGURATION_FS_STRING "heimdall-helm CDC Config"
#define USBD_INTERFACE_FS_STRING "heimdall-helm CDC Interface"

#define DEVICE_ID1 (UID_BASE)
#define DEVICE_ID2 (UID_BASE + 0x4U)
#define DEVICE_ID3 (UID_BASE + 0x8U)
#define USB_SIZ_STRING_SERIAL 0x1AU

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *lang_id_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *product_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *config_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static void int_to_unicode(uint32_t value, uint8_t *pbuf, uint8_t len);
static void fill_serial_number(void);

USBD_DescriptorsTypeDef HELM_USBD_Desc = {
    device_descriptor,
    lang_id_descriptor,
    manufacturer_descriptor,
    product_descriptor,
    serial_descriptor,
    config_descriptor,
    interface_descriptor,
};

__ALIGN_BEGIN static uint8_t deviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,                 /* bLength */
    USB_DESC_TYPE_DEVICE,  /* bDescriptorType */
    0x00, 0x02,            /* bcdUSB = 2.00 */
    /* bDeviceClass = 0x02 (CDC), not 0x00 ("defined per-interface") --
       bench-confirmed bug: 0x00 without an Interface Association
       Descriptor left Windows unable to tell the Control and Data
       interfaces belong to one CDC-ACM function (ConfigManagerErrorCode
       28 "drivers not installed" on the Data interface, 10 "cannot
       start" on the Control interface that would otherwise get
       usbser.sys). Matches Betaflight's real, deployed H7 device
       descriptor (src/platform/STM32/vcp_hal/usbd_desc.c), not derived
       here. */
    0x02,                  /* bDeviceClass */
    0x00,                  /* bDeviceSubClass */
    0x00,                  /* bDeviceProtocol */
    USB_MAX_EP0_SIZE,      /* bMaxPacketSize0 */
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID), HIBYTE(USBD_PID),
    0x00, 0x02,            /* bcdDevice = 2.00 */
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION,
};

__ALIGN_BEGIN static uint8_t langIdDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING), HIBYTE(USBD_LANGID_STRING),
};

__ALIGN_BEGIN static uint8_t stringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END = {
    USB_SIZ_STRING_SERIAL,
    USB_DESC_TYPE_STRING,
};

__ALIGN_BEGIN static uint8_t strDescScratch[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

/* ---- USBD_DescriptorsTypeDef callbacks below: ST's fixed template shape
   for a "return length + pointer to a static descriptor buffer" accessor,
   one per HELM_USBD_Desc field above -- not worth individually commenting,
   see this file's own header comment for what's actually project-specific
   about them (the VID/PID/string values, not this accessor shape). ---- */

static uint8_t *device_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(deviceDesc);
    return deviceDesc;
}

static uint8_t *lang_id_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = sizeof(langIdDesc);
    return langIdDesc;
}

static uint8_t *manufacturer_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, strDescScratch, length);
    return strDescScratch;
}

static uint8_t *product_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)USBD_PRODUCT_FS_STRING, strDescScratch, length);
    return strDescScratch;
}

static uint8_t *serial_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    *length = USB_SIZ_STRING_SERIAL;
    fill_serial_number();
    return stringSerial;
}

static uint8_t *config_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_FS_STRING, strDescScratch, length);
    return strDescScratch;
}

static uint8_t *interface_descriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    (void)speed;
    USBD_GetString((uint8_t *)USBD_INTERFACE_FS_STRING, strDescScratch, length);
    return strDescScratch;
}

/* Builds this unit's USB serial-number string from the chip's 96-bit
   unique ID (folded down to two 32-bit halves), so every physical board
   enumerates with its own distinct serial rather than a shared constant. */
static void fill_serial_number(void) {
    uint32_t const id0 = *(uint32_t *)DEVICE_ID1 + *(uint32_t *)DEVICE_ID3;
    uint32_t const id1 = *(uint32_t *)DEVICE_ID2;

    int_to_unicode(id0, &stringSerial[2], 8U);
    int_to_unicode(id1, &stringSerial[18], 4U);
}

/* Writes value's most-significant len nibbles as hex ASCII characters
   into pbuf, each padded to a UTF-16LE code unit (low byte = char, high
   byte = 0) as USB string descriptors require. */
static void int_to_unicode(uint32_t value, uint8_t *pbuf, uint8_t len) {
    for (uint8_t idx = 0U; idx < len; idx++) {
        uint8_t const nibble = (uint8_t)(value >> 28);
        pbuf[2U * idx] = (nibble < 0xAU) ? (uint8_t)(nibble + '0') : (uint8_t)(nibble + 'A' - 10U);
        pbuf[2U * idx + 1U] = 0U;
        value <<= 4;
    }
}
