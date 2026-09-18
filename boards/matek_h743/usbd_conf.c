#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_cdc.h"

/* USB Device low-level driver for this board -- the HAL_PCD_* glue that
   ST's usbd_conf_template.c (Middlewares/ST/STM32_USB_Device_Library/
   Core/Src/usbd_conf_template.c) leaves as empty stubs for the
   integrator to fill in. There's no CubeMX/CubeIDE project in this repo
   to generate it from (deliberately -- see this project's own
   platformio.ini header on why framework=stm32cube stays raw HAL, no
   generated glue), so this is hand-written against real ST reference
   values (endpoint numbers/FIFO sizes match ST's own CDC class defaults
   and CubeMX's standard H7-FS-CDC FIFO split) rather than invented.
   Needs real bench verification -- no debug probe/USB analyzer available
   in the environment this was written in, see ../../.docs/hardware.md.

   USB peripheral: OTG_FS (USB2_OTG_FS on H743, PA11/PA12, internal FS
   PHY) -- board.c's own system_clock_config() already anticipated this,
   see its RCC_PERIPHCLK_USB/HSI48+CRS block synced to
   RCC_CRS_SYNC_SOURCE_USB2. VBUS sensing is left disabled: no VBUS-sense
   GPIO is confirmed wired on this board (no schematic in this repo, see
   .docs/hardware.md), and disabling it is the standard safe default for
   a self/bus-powered board without one. */

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* ---- USBD_LL_* : bridges the USB Device middleware to HAL_PCD ---- */

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev) {
    hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
    hpcd_USB_OTG_FS.Init.dev_endpoints = 4;
    hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.battery_charging_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
    hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
    hpcd_USB_OTG_FS.Init.use_external_vbus = DISABLE;
    hpcd_USB_OTG_FS.Init.ep0_mps = 64;

    /* Link pdev <-> hpcd both ways, same as CubeMX-generated usbd_conf.c
       -- HAL_PCD_*Callback below reach back into the USBD_LL_* layer via
       hpcd->pData, and USBD_LL_Transmit/PrepareReceive reach HAL_PCD via
       this same hpcd_USB_OTG_FS global. */
    hpcd_USB_OTG_FS.pData = pdev;
    pdev->pData = &hpcd_USB_OTG_FS;

    if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK) {
        return USBD_FAIL;
    }

    /* OTG FIFO split (in 32-bit words) for one CDC interface: EP0 TX,
       CDC_CMD (interrupt IN), CDC_DATA (bulk IN) -- ST's own standard
       CubeMX-generated values for an H7 FS device with a single CDC
       class, not derived here. */
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x80);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x40);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x80);
    HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2, 0x10);

    return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    return (HAL_PCD_DeInit(&hpcd_USB_OTG_FS) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    return (HAL_PCD_Start(&hpcd_USB_OTG_FS) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev) {
    (void)pdev;
    return (HAL_PCD_Stop(&hpcd_USB_OTG_FS) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps) {
    (void)pdev;
    return (HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, ep_addr, ep_mps, ep_type) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_Close(&hpcd_USB_OTG_FS, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_Flush(&hpcd_USB_OTG_FS, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_SetStall(&hpcd_USB_OTG_FS, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return (HAL_PCD_EP_ClrStall(&hpcd_USB_OTG_FS, ep_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    if ((ep_addr & 0x80U) != 0U) {
        return hpcd_USB_OTG_FS.IN_ep[ep_addr & 0x7FU].is_stall;
    }
    return hpcd_USB_OTG_FS.OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr) {
    (void)pdev;
    return (HAL_PCD_SetAddress(&hpcd_USB_OTG_FS, dev_addr) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size) {
    (void)pdev;
    return (HAL_PCD_EP_Transmit(&hpcd_USB_OTG_FS, ep_addr, pbuf, size) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size) {
    (void)pdev;
    return (HAL_PCD_EP_Receive(&hpcd_USB_OTG_FS, ep_addr, pbuf, size) == HAL_OK) ? USBD_OK : USBD_FAIL;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
    (void)pdev;
    return HAL_PCD_EP_GetRxCount(&hpcd_USB_OTG_FS, ep_addr);
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

/* ---- HAL_PCD_MspInit/MspDeInit : GPIO + clock + NVIC for OTG_FS ---- */

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd) {
    (void)hpcd;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA11 (DM), PA12 (DP) -- OTG_FS's fixed pins on this package, AF10.
       No ID pin (device-only, no OTG role switching) and no VBUS-sense
       pin wired (vbus_sensing_enable is DISABLE above). */
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpioInit.Mode = GPIO_MODE_AF_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpioInit.Alternate = GPIO_AF10_OTG2_FS;
    HAL_GPIO_Init(GPIOA, &gpioInit);

    __HAL_RCC_USB2_OTG_FS_CLK_ENABLE();

    HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd) {
    (void)hpcd;

    HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
    __HAL_RCC_USB2_OTG_FS_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
}

/* ---- HAL_PCD_*Callback : bridge PCD events into USBD_LL_* ---- */

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum) {
    USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd) {
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;
    USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, speed);
    USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd) {
    USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

/* Weak override of the CMSIS startup file's default handler
   (startup_stm32h743xx.s : OTG_FS_IRQHandler -> Default_Handler). */
void OTG_FS_IRQHandler(void) {
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}
