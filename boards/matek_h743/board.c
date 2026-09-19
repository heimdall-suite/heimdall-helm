#include "board.h"
#include "stm32h7xx_hal.h"
#include <stdbool.h>
#include <string.h>

/* Clock tree: 8MHz HSE, PLL1 -> 480MHz (REV_ID_V silicon) or 400MHz
   (older silicon), HCLK = sysclk/2, APB1-4 = HCLK/2.

   Ported from aoa-boat-controller's src/H743/clock_config.cpp
   (SystemClock_Config() override for devebox_h743vitx's own template,
   which assumes a 25MHz HSE this board doesn't have). That file's own
   comments carry the full provenance -- summarized here:

   - 8MHz HSE: cross-checked against two independent, real, deployed
     firmware targets for this exact board (ArduPilot's MatekH743/
     hwdef.dat: OSCILLATOR_HZ 8000000; Betaflight's top-level Makefile:
     HSE_VALUE ?= 8000000).
   - Silicon-revision-gated 480MHz/400MHz split, FLASH_LATENCY_2, and the
     HSI48+CRS USB clock approach: all Betaflight's real, working values
     for this MCU family (STM32H743/H750/H757), not independently derived.
   - The PLL M/N/P divider values themselves (M=1, N=60 or 50, P=1) ARE
     this project's own arithmetic for an 8MHz input -- not sourced from a
     working reference. Bench-confirmed on aoa-boat-controller's H743 unit
     (HAL_RCC_GetSysClockFreq() read back correctly, per its decisions.md),
     but re-verify against RM0433 if this ever moves to different silicon. */
static void system_clock_config(void) {
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    bool const isRevV = (HAL_GetREVID() == REV_ID_V);

    __HAL_PWR_VOLTAGESCALING_CONFIG(isRevV ? PWR_REGULATOR_VOLTAGE_SCALE0
                                            : PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
        /* wait for the regulator to settle at the new voltage scale */
    }

    RCC_OscInitTypeDef oscInit = {0};
    oscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    oscInit.HSEState = RCC_HSE_ON;
    oscInit.HSI48State = RCC_HSI48_ON;

    oscInit.PLL.PLLState = RCC_PLL_ON;
    oscInit.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscInit.PLL.PLLM = 1;
    oscInit.PLL.PLLN = isRevV ? 60 : 50;
    oscInit.PLL.PLLP = 1;
    oscInit.PLL.PLLQ = 2;
    oscInit.PLL.PLLR = 2;
    oscInit.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    oscInit.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    oscInit.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&oscInit) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitTypeDef clkInit = {0};
    clkInit.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clkInit.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clkInit.AHBCLKDivider = RCC_HCLK_DIV2;
    clkInit.APB3CLKDivider = RCC_APB3_DIV2;
    clkInit.APB1CLKDivider = RCC_APB1_DIV2;
    clkInit.APB2CLKDivider = RCC_APB2_DIV2;
    clkInit.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&clkInit, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }

    RCC_PeriphCLKInitTypeDef periphClkInit = {0};
    periphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    periphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInit) != HAL_OK) {
        Error_Handler();
    }

    RCC_CRSInitTypeDef crsInit = {0};
    crsInit.Prescaler = RCC_CRS_SYNC_DIV1;
    crsInit.Source = RCC_CRS_SYNC_SOURCE_USB2;
    crsInit.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    crsInit.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
    crsInit.ErrorLimitValue = 34;
    crsInit.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
    HAL_RCCEx_CRSConfig(&crsInit);
}

/* PE3 "CAL" LED -- see board_led_toggle()'s own comment in board.h for
   the pin/polarity source. */
static void led_init(void) {
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_3;
    gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpioInit);

    /* active-low: start off */
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, GPIO_PIN_SET);
}

void board_led_toggle(void) {
    HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_3);
}

/* SBUS UART -- see board.h's own comment on board_sbus_uart_init() for the
   pin/inverter provenance and why DMA+idle over a byte interrupt. */

static UART_HandleTypeDef sbusUart;
static DMA_HandleTypeDef sbusUartDma;

static uint8_t sbusCaptureBuf[SBUS_UART_FRAME_LEN];
static uint8_t sbusLatestFrame[SBUS_UART_FRAME_LEN];
static volatile bool sbusFrameReady = false;

/* Re-arms one capture: up to SBUS_UART_FRAME_LEN bytes, stopping early on
   an idle-line gap. Called after every completed or discarded capture --
   HAL_UARTEx_ReceiveToIdle_DMA is one-shot, it doesn't auto-continue.
   No-ops (returns HAL_BUSY) if a capture is already in flight -- see its
   only other call site, in HAL_UARTEx_RxEventCallback below, for why that
   can happen and is fine. */
static void sbus_uart_rearm(void) {
    HAL_UARTEx_ReceiveToIdle_DMA(&sbusUart, sbusCaptureBuf, SBUS_UART_FRAME_LEN);
}

/* HAL callback (weak override): fires once per DMA half-transfer, once
   per completed idle-line/full capture. The half-transfer case has to be
   explicitly ignored, not just filtered by size -- HAL_UARTEx_
   ReceiveToIdle_DMA's own half-transfer notification fires at exactly
   half of SBUS_UART_FRAME_LEN (huart->RxXferSize / 2, confirmed in
   stm32h7xx_hal_uart.c's UART_DMARxHalfCplt) partway through *every*
   normal frame, not just a real short/corrupted one -- checking
   huart->RxEventType is the only way to tell "still mid-transfer" apart
   from "capture actually ended here" (idle or full). Re-arming on that
   spurious half-transfer event would harmlessly no-op (the real transfer
   is still running, RxState isn't READY yet), but there is no reason to
   attempt it. */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
    if (huart->Instance != USART6 || huart->RxEventType == HAL_UART_RXEVENT_HT) {
        return;
    }

    if (size == SBUS_UART_FRAME_LEN) {
        memcpy(sbusLatestFrame, sbusCaptureBuf, SBUS_UART_FRAME_LEN);
        sbusFrameReady = true;
    }
    /* A short capture (idle fired early) is silently dropped -- see
       board_sbus_uart_take_frame()'s own header comment in board.h for
       why that's already self-resynchronizing. */

    sbus_uart_rearm();
}

/* HAL callback (weak override): fires when HAL_UART_IRQHandler sees a
   parity/framing/noise/overrun error. Bench-found (not anticipated) bug:
   because reception runs over DMA, HAL treats every one of those as a
   "blocking" error (stm32h7xx_hal_uart.c's HAL_UART_IRQHandler -- any
   error while USART_CR3_DMAR is set) and aborts the DMA reception
   entirely before calling this -- the default weak implementation does
   nothing, so without this override, one error (e.g. line noise from a
   momentarily floating PC7 while the SBUS receiver is unplugged)
   permanently stops all future reception, even after the receiver is
   reconnected and transmitting cleanly again. Same fix as a short/
   discarded capture: just re-arm and let the next real idle-line gap
   resynchronize things, no special recovery logic needed. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART6) {
        return;
    }

    sbus_uart_rearm();
}

/* HAL callback (weak override): GPIO/clock/DMA-link/NVIC for whichever
   UART instance HAL_UART_Init() is bringing up -- same role as
   usbd_conf.c's HAL_PCD_MspInit for OTG_FS. Only USART6 exists on this
   board today; the instance check just keeps this correct if that ever
   changes, at zero extra ceremony. */
void HAL_UART_MspInit(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART6) {
        return;
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* PC7 ("RX6" silk), RX only -- SBUS receivers only ever transmit to
       the FC, so USART6's TX pin (PC6) is left unconfigured. */
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_7;
    gpioInit.Mode = GPIO_MODE_AF_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInit.Alternate = GPIO_AF7_USART6;
    HAL_GPIO_Init(GPIOC, &gpioInit);

    /* DMA1 Stream0 -- arbitrary, first free stream; nothing else on this
       board uses DMA yet. DMA_REQUEST_USART6_RX is DMAMUX1 request 71,
       fixed by silicon, not a board choice. */
    sbusUartDma.Instance = DMA1_Stream0;
    sbusUartDma.Init.Request = DMA_REQUEST_USART6_RX;
    sbusUartDma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    sbusUartDma.Init.PeriphInc = DMA_PINC_DISABLE;
    sbusUartDma.Init.MemInc = DMA_MINC_ENABLE;
    sbusUartDma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    sbusUartDma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    sbusUartDma.Init.Mode = DMA_NORMAL;
    sbusUartDma.Init.Priority = DMA_PRIORITY_MEDIUM;
    sbusUartDma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&sbusUartDma) != HAL_OK) {
        Error_Handler();
    }
    __HAL_LINKDMA(huart, hdmarx, sbusUartDma);

    /* Priority 5: this project's established floor for any ISR that runs
       alongside FreeRTOS (matches usbd_conf.c's OTG_FS_IRQn) --
       configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (FreeRTOSConfig.h) is
       5, and an interrupt numerically below that must never call a
       FreeRTOS API. Neither ISR here calls one today, but keeping every
       peripheral ISR at or below this floor by default avoids that
       becoming a live bug the day one of them starts publishing into a
       queue from ISR context. */
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
}

/* Weak overrides of the CMSIS startup file's default handlers
   (startup_stm32h743xx.s : DMA1_Stream0_IRQHandler/USART6_IRQHandler ->
   Default_Handler), same pattern as usbd_conf.c's OTG_FS_IRQHandler. */
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&sbusUartDma);
}

void USART6_IRQHandler(void) {
    HAL_UART_IRQHandler(&sbusUart);
}

void board_sbus_uart_init(void) {
    sbusUart.Instance = USART6;
    sbusUart.Init.BaudRate = 100000;
    /* SBUS is 8E2 (8 data bits, even parity, 2 stop bits). On every
       STM32 UART, an enabled parity bit is counted as part of
       WordLength, not added on top of it -- 8 data + 1 parity needs
       WordLength = 9B, not 8B (architectural HAL behavior, not a
       per-chip detail: same convention on F1/F4/F7/H7). */
    sbusUart.Init.WordLength = UART_WORDLENGTH_9B;
    sbusUart.Init.StopBits = UART_STOPBITS_2;
    sbusUart.Init.Parity = UART_PARITY_EVEN;
    sbusUart.Init.Mode = UART_MODE_RX;
    sbusUart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    sbusUart.Init.OverSampling = UART_OVERSAMPLING_16;
    sbusUart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    sbusUart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    /* Inverted line, no external/onboard inverter on this board -- see
       board.h's comment. */
    sbusUart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_RXINVERT_INIT;
    sbusUart.AdvancedInit.RxPinLevelInvert = UART_ADVFEATURE_RXINV_ENABLE;

    if (HAL_UART_Init(&sbusUart) != HAL_OK) {
        Error_Handler();
    }

    sbus_uart_rearm();
}

bool board_sbus_uart_take_frame(uint8_t out[SBUS_UART_FRAME_LEN]) {
    if (!sbusFrameReady) {
        return false;
    }

    /* Short, bounded critical section against HAL_UARTEx_RxEventCallback
       running in ISR context -- see that callback above. */
    __disable_irq();
    memcpy(out, sbusLatestFrame, SBUS_UART_FRAME_LEN);
    sbusFrameReady = false;
    __enable_irq();

    return true;
}

/* IWDG1 -- issue #5. STM32H7's IWDG is clocked from LSI regardless of
   the main clock tree (RM0433, IWDG chapter), and HAL_IWDG_Init's
   __HAL_IWDG_START forces LSI on itself -- no RCC LSI setup needed here
   the way system_clock_config() sets up HSE/PLL.

   Prescaler /32 against LSI_VALUE's nominal 32kHz (stm32h7xx_hal_conf.h)
   gives a 1ms tick; Reload 249 (250 ticks) is a ~250ms nominal timeout --
   roughly 25x SUPERVISOR_POLL_PERIOD_MS's 10ms sweep, so ordinary
   scheduling jitter across other tasks can't false-trip it, while a
   genuinely wedged supervisor still forces a reset well under a second.
   Not bench-verified against real LSI drift yet (RM0433 gives it a wide
   tolerance across temperature) -- revisit if bench testing shows the
   margin is wrong in either direction. */
static IWDG_HandleTypeDef iwdg;

void board_iwdg_init(void) {
    iwdg.Instance = IWDG1;
    iwdg.Init.Prescaler = IWDG_PRESCALER_32;
    iwdg.Init.Reload = 249;
    iwdg.Init.Window = IWDG_WINDOW_DISABLE;
    if (HAL_IWDG_Init(&iwdg) != HAL_OK) {
        Error_Handler();
    }
}

void board_iwdg_refresh(void) {
    HAL_IWDG_Refresh(&iwdg);
}

void board_init(void) {
    system_clock_config();
    led_init();
}
