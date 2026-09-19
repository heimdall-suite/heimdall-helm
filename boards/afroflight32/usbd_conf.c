#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_cdc.h"

/* USB Device low-level driver for this board -- the HAL_PCD_* glue that
   ST's usbd_conf_template.c (Middlewares/ST/STM32_USB_Device_Library/
   Core/Src/usbd_conf_template.c) leaves as empty stubs for the
   integrator to fill in, same role as boards/matek_h743/usbd_conf.c but
   for a fundamentally different USB peripheral: STM32F103's plain "USB"
   full-speed device controller (PMA-based single-buffered endpoints,
   register set: KR/EPnR/DADDR/BTABLE), not OTG_FS -- no FIFO split, no
   dedicated-EP1/VBUS-sensing/battery-charging fields even exist on this
   peripheral's HAL_PCD_Init struct (confirmed against
   framework-stm32cubef1's stm32f1xx_ll_usb.h before writing this, not
   assumed to match H7's).

   No CubeMX/CubeIDE project in this repo to generate this from (see
   platformio.ini's own header on why), and no confirmed prior bench-
   verified F103 USB-CDC source in this project's sibling
   aoa-boat-controller was available to check against either (not
   present in this environment). Instead, ported line-for-line from ST's
   own official reference for this exact USB peripheral -- STM32CubeF1's
   STM3210E_EVAL/Applications/USB_Device/CDC_Standalone/Src/usbd_conf.c
   (github.com/STMicroelectronics/STM32CubeF1), a real, ST-shipped,
   presumably-tested example for a Medium/High-density F103 with this
   same "USB" peripheral -- not invented here. Two deliberate deviations
   from that source, both called out at their own site below: the NVIC
   priority (this project's own established floor, not ST's raw example
   value) and the GPIOB/PB14 "USB_DISCONNECT" software pull-up control
   (that eval board's OWN schematic detail, dropped entirely rather than
   guessed at for a board whose actual D+ pull-up wiring isn't confirmed
   here -- see this file's NOTE below).

   NEEDS REAL BENCH VERIFICATION -- not yet flashed/tested on the actual
   afroflight32 unit (issue #12 is the transport only; nothing calls
   usb_cdc_init() yet until #13 wires up the CLI that runs over it). If
   the device never enumerates on the bench, the first thing to check is
   exactly the pull-up question this file's NOTE flags, not the PCD/PMA
   config below (that part is ST's own tested values, not a likely
   culprit). */

/* NOTE, unconfirmed: ST's STM3210E_EVAL reference this file is ported
   from drives a GPIO (PB14, "USB_DISCONNECT" in its own schematic) to
   switch a transistor-gated 1.5k D+ pull-up under software control, via
   a HAL_PCDEx_SetConnectionState() override. This board's actual D+
   pull-up wiring isn't confirmed here (no schematic in this repo, see
   ../../.docs/hardware.md) -- many simpler F103 boards instead wire a
   FIXED pull-up resistor directly to PA12 with no software control at
   all, which is what's assumed here: HAL_PCDEx_SetConnectionState() is
   deliberately NOT overridden, falling back to the HAL's own __weak
   no-op default (stm32f1xx_hal_pcd_ex.c) -- the safe default absent
   confirmed wiring, same reasoning as matek_h743/usbd_conf.c's own
   VBUS-sensing comment. If the board turns out to need the switched-
   pull-up scheme instead, enumeration will simply never happen (not a
   crash, not a HAL error -- HAL_PCD_Start() succeeds either way), and
   this is the first thing to revisit. */

PCD_HandleTypeDef hpcd;

/* ---- USBD_LL_* : bridges the USB Device middleware to HAL_PCD ---- */

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev) {
    hpcd.Instance = USB;
    hpcd.Init.dev_endpoints = 8;
    hpcd.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd.Init.speed = PCD_SPEED_FULL;
    hpcd.Init.low_power_enable = 0;

    hpcd.pData = pdev;
    pdev->pData = &hpcd;

    if (HAL_PCD_Init(&hpcd) != HAL_OK) {
        return USBD_FAIL;
    }

    /* PMA (Packet Memory Area) buffer layout, in bytes -- one single-
       buffered region per endpoint, non-overlapping. ST's own values for
       this exact CDC class instance (EP0 IN/OUT, then CDC_IN_EP/
       CDC_OUT_EP/CDC_CMD_EP as usbd_cdc.h defines them), not derived
       here. */
    HAL_PCDEx_PMAConfig(pdev->pData, 0x00, PCD_SNG_BUF, 0x40);
    HAL_PCDEx_PMAConfig(pdev->pData, 0x80, PCD_SNG_BUF, 0x80);
    HAL_PCDEx_PMAConfig(pdev->pData, CDC_IN_EP, PCD_SNG_BUF, 0xC0);
    HAL_PCDEx_PMAConfig(pdev->pData, CDC_OUT_EP, PCD_SNG_BUF, 0x110);
    HAL_PCDEx_PMAConfig(pdev->pData, CDC_CMD_EP, PCD_SNG_BUF, 0x100);

    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    return (HAL_PCD_DeInit(&hpcd) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    return (HAL_PCD_Start(&hpcd) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    return (HAL_PCD_Stop(&hpcd) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps) {
    (void)pdev;
    return (HAL_PCD_EP_Open(&hpcd, ep_addr, ep_mps, ep_type) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_Close(&hpcd, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_Flush(&hpcd, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_SetStall(&hpcd, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_ClrStall(&hpcd, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    if ((ep_addr & 0x80U) != 0U) {
        return hpcd.IN_ep[ep_addr & 0x7FU].is_stall;
    }
    return hpcd.OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr) {
    (void)pdev;
    return (HAL_PCD_SetAddress(&hpcd, dev_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size) {
    (void)pdev;
    return (HAL_PCD_EP_Transmit(&hpcd, ep_addr, pbuf, size) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size) {
    (void)pdev;
    return (HAL_PCD_EP_Receive(&hpcd, ep_addr, pbuf, size) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return HAL_PCD_EP_GetRxCount(&hpcd, ep_addr);
}

void *USBD_static_malloc(uint32_t size) {
    (void)size;
    static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef) / 4U) + 1U];
    return mem;
}

void USBD_static_free(void *p) {
    (void)p;
}

void USBD_LL_Delay(uint32_t Delay) {
    HAL_Delay(Delay);
}

/* ---- HAL_PCD_MspInit/MspDeInit : GPIO + clock + NVIC for USB ---- */

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd_) {
    (void)hpcd_;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA11 (DM), PA12 (DP) -- USB's fixed pins on every F103 package, no
       AFIO remap option (unlike some other F103 peripherals). AF_INPUT +
       PULLUP, not AF_PP: ST's own reference config for this exact
       peripheral (STM3210E_EVAL's usbd_conf.c) -- the USB peripheral
       drives these pins itself once enabled, this GPIO config just gets
       them out of the way. */
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpioInit.Mode = GPIO_MODE_AF_INPUT;
    gpioInit.Pull = GPIO_PULLUP;
    gpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpioInit);

    __HAL_RCC_USB_CLK_ENABLE();

    /* Priority 5, not ST's raw example's 7: this project's own
       established floor for any ISR that runs alongside FreeRTOS --
       configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (FreeRTOSConfig.h) is
       5 on this board too (same value as matek_h743's), and an interrupt
       numerically below that must never call a FreeRTOS API. This ISR
       doesn't today (see lib/usb_cdc/stm32f1.c's cdc_receive -- plain
       ring-buffer writes, no queue), but keeping every peripheral ISR at
       or below this floor by default avoids that becoming a live bug
       the day it does, same reasoning as matek_h743/board.c's own UART
       IRQ priority comment. */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd_) {
    (void)hpcd_;

    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    __HAL_RCC_USB_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
}

/* ---- HAL_PCD_*Callback : bridge PCD events into USBD_LL_* ---- */

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd_) {
    USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd_->pData, (uint8_t *)hpcd_->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd_, uint8_t epnum) {
    USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd_->pData, epnum, hpcd_->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd_, uint8_t epnum) {
    USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd_->pData, epnum, hpcd_->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd_) {
    USBD_LL_SOF((USBD_HandleTypeDef *)hpcd_->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd_) {
    USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd_->pData, USBD_SPEED_FULL);
    USBD_LL_Reset((USBD_HandleTypeDef *)hpcd_->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd_) {
    USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd_->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd_) {
    USBD_LL_Resume((USBD_HandleTypeDef *)hpcd_->pData);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd_) {
    USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd_->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd_) {
    USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd_->pData);
}

/* Weak override of the CMSIS startup file's default handler
   (startup_stm32f103xb.s : USB_LP_CAN1_RX0_IRQHandler -> Default_Handler).
   Shared with CAN1's RX0 interrupt on silicon that has both peripherals
   (confirmed for STM32F103xB in this framework's own CMSIS header) --
   this board doesn't use CAN, so no conflict here. */
void USB_LP_CAN1_RX0_IRQHandler(void) {
    HAL_PCD_IRQHandler(&hpcd);
}
