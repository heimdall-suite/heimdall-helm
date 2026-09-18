#ifndef HELM_BOARD_MATEK_H743_USBD_DESC_H
#define HELM_BOARD_MATEK_H743_USBD_DESC_H

#include "usbd_def.h"

/* This board's descriptor callback table, passed to USBD_Init() by
   lib/usb_cdc/stm32h7.c's usb_cdc_init(). Defined in usbd_desc.c. */
extern USBD_DescriptorsTypeDef HELM_USBD_Desc;

#endif /* HELM_BOARD_MATEK_H743_USBD_DESC_H */
