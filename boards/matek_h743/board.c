#include "board.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_ll_usart.h"
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
    if (huart->Instance == UART7) {
        /* S.Port -- PE8, "TX7" silk. GPIO/clock/NVIC only, no DMA: this
           peripheral is driven byte-by-byte from UART7_IRQHandler
           itself (board_sport_uart_init()'s own comment), not through
           HAL's transmit/receive path. */
        __HAL_RCC_GPIOE_CLK_ENABLE();
        __HAL_RCC_UART7_CLK_ENABLE();

        GPIO_InitTypeDef gpioInit = {0};
        gpioInit.Pin = GPIO_PIN_8;
        gpioInit.Mode = GPIO_MODE_AF_PP;
        gpioInit.Pull = GPIO_PULLUP;
        gpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
        gpioInit.Alternate = GPIO_AF7_UART7;
        HAL_GPIO_Init(GPIOE, &gpioInit);

        HAL_NVIC_SetPriority(UART7_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(UART7_IRQn);
        return;
    }

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

/* S.Port UART -- UART7/PE8. See board.h's own comment for the full
   provenance (ported from aoa-boat-controller's bench-verified
   SportUart) and why this bypasses HAL_UART_HandleTypeDef entirely for
   this one peripheral. */

#define SPORT_UART_RING_SIZE 32

static volatile uint8_t sportRxBuf[SPORT_UART_RING_SIZE];
static volatile uint8_t sportRxHead;
static volatile uint8_t sportRxTail;

static volatile uint8_t sportTxBuf[SPORT_UART_RING_SIZE];
static volatile uint8_t sportTxHead;
static volatile uint8_t sportTxTail;

static TaskHandle_t sportRxTask;

/* CR1: clear TE|RE, then set only RE -- idle/listening state. */
static void sport_uart_enable_receive(void) {
    CLEAR_BIT(UART7->CR1, USART_CR1_TE | USART_CR1_RE);
    SET_BIT(UART7->CR1, USART_CR1_RE);
}

/* CR1: clear TE|RE, then set only TE -- about to drive the line. */
static void sport_uart_enable_transmit(void) {
    CLEAR_BIT(UART7->CR1, USART_CR1_TE | USART_CR1_RE);
    SET_BIT(UART7->CR1, USART_CR1_TE);
}

void board_sport_uart_set_rx_task(TaskHandle_t task) {
    sportRxTask = task;
}

void board_sport_uart_init(void) {
    /* HAL_HalfDuplex_Init only for the one-time peripheral config (it
       reads the actual configured clock tree for BRR rather than
       needing hand-derived baud math, same reasoning board_sbus_uart_
       init() relies on HAL_UART_Init for, and triggers HAL_UART_MspInit
       above for GPIO/clock/NVIC) -- LL_USART_Init/LL_USART_InitTypeDef
       would need USE_FULL_LL_DRIVER, which this project doesn't define,
       so this reaches the same register state through HAL's init path
       instead. Everything AFTER this call bypasses HAL entirely (no
       HAL_UART_Transmit_IT -- see board.h's comment on why); the LL
       bit-level calls/macros used below and in UART7_IRQHandler are
       plain inline register accessors, not gated behind
       USE_FULL_LL_DRIVER the way the *_Init family is. */
    UART_HandleTypeDef sportUart = {0};
    sportUart.Instance = UART7;
    sportUart.Init.BaudRate = 57600; /* S.Port's fixed rate */
    sportUart.Init.WordLength = UART_WORDLENGTH_8B;
    sportUart.Init.StopBits = UART_STOPBITS_1;
    sportUart.Init.Parity = UART_PARITY_NONE;
    sportUart.Init.Mode = UART_MODE_TX_RX;
    sportUart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    sportUart.Init.OverSampling = UART_OVERSAMPLING_16;
    sportUart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    sportUart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    /* S.Port is electrically inverted -- both directions, since this is
       a real bus (SBUS's own RXINV-only AdvancedInit is one direction
       for the same reason -- see board_sbus_uart_init()'s comment). */
    sportUart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_TXINVERT_INIT | UART_ADVFEATURE_RXINVERT_INIT;
    sportUart.AdvancedInit.TxPinLevelInvert = UART_ADVFEATURE_TXINV_ENABLE;
    sportUart.AdvancedInit.RxPinLevelInvert = UART_ADVFEATURE_RXINV_ENABLE;

    if (HAL_HalfDuplex_Init(&sportUart) != HAL_OK) {
        Error_Handler();
    }

    /* No overrun interrupt wired up -- this bus is low-traffic single-
       byte polls; a slow reader just keeps the newest bytes instead of
       needing an error-flag-clear path. Matches the reference driver. */
    LL_USART_DisableOverrunDetect(UART7);

    /* Idle state: listening -- narrows HAL_HalfDuplex_Init's TX_RX mode
       down to RE-only. Every CR1 TE/RE change from here on goes through
       this and sport_uart_enable_transmit() directly, never back
       through HAL. */
    sport_uart_enable_receive();

    /* RX stays armed permanently -- poll detection needs every byte.
       TXEIE/TCIE are left off here; board_sport_uart_write()/the ISR
       arm them only around an actual send. */
    LL_USART_EnableIT_RXNE(UART7);
}

bool board_sport_uart_available(void) {
    return sportRxHead != sportRxTail;
}

uint8_t board_sport_uart_read_byte(void) {
    uint8_t const b = sportRxBuf[sportRxTail];
    sportRxTail = (uint8_t)((sportRxTail + 1) % SPORT_UART_RING_SIZE);
    return b;
}

void board_sport_uart_write(const uint8_t *data, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        uint8_t const nextHead = (uint8_t)((sportTxHead + 1) % SPORT_UART_RING_SIZE);
        while (nextHead == sportTxTail) {
            /* Ring buffer full -- wait for the ISR to drain space. S.Port
               frames are far smaller than SPORT_UART_RING_SIZE, so this
               should never actually spin in practice (same assumption
               the reference driver makes). */
        }
        sportTxBuf[sportTxHead] = data[i];
        sportTxHead = nextHead;
    }

    /* Switch to drive mode before arming the interrupt that starts
       feeding it, so there's no window where TXE could fire while still
       listening. The ISR switches back once the whole frame has
       genuinely finished (TC, not just TXE) -- see board.h's comment. */
    sport_uart_enable_transmit();
    LL_USART_EnableIT_TXE(UART7);
}

void UART7_IRQHandler(void) {
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    /* RX: pull any received byte into the ring buffer and wake the
       registered task -- see board_sport_uart_set_rx_task(). Only
       meaningful while RE is active (idle/listening). A full buffer
       drops the byte rather than overwriting unread data; the notified
       task drains every tick, so this realistically never fills. */
    if (LL_USART_IsActiveFlag_RXNE(UART7)) {
        uint8_t const b = (uint8_t)UART7->RDR;
        uint8_t const nextHead = (uint8_t)((sportRxHead + 1) % SPORT_UART_RING_SIZE);
        if (nextHead != sportRxTail) {
            sportRxBuf[sportRxHead] = b;
            sportRxHead = nextHead;
        }
        if (sportRxTask != NULL) {
            vTaskNotifyGiveFromISR(sportRxTask, &higherPriorityTaskWoken);
        }
    }

    /* TX: while TXEIE is armed and the shift register is ready, either
       send the next queued byte or, once the buffer is empty, disable
       TXEIE and arm TCIE instead -- no chunking, no per-byte software
       re-arm, the interrupt just keeps re-firing on its own every time
       the shift register empties, for as long as TXEIE stays set. */
    if (LL_USART_IsEnabledIT_TXE(UART7) && LL_USART_IsActiveFlag_TXE(UART7)) {
        if (sportTxTail == sportTxHead) {
            /* Nothing left to queue -- but the last byte handed to the
               shift register is still physically shifting out. TC (not
               TXE) confirms it's actually gone. */
            LL_USART_DisableIT_TXE(UART7);
            SET_BIT(UART7->CR1, USART_CR1_TCIE);
        } else {
            UART7->TDR = sportTxBuf[sportTxTail];
            sportTxTail = (uint8_t)((sportTxTail + 1) % SPORT_UART_RING_SIZE);
        }
    }

    /* Transmission genuinely complete -- switch back to listening. Only
       reached once TCIE was armed above, never spuriously on a stale
       flag from before a send started. */
    if ((UART7->CR1 & USART_CR1_TCIE) && LL_USART_IsActiveFlag_TC(UART7)) {
        LL_USART_ClearFlag_TC(UART7);
        CLEAR_BIT(UART7->CR1, USART_CR1_TCIE);
        sport_uart_enable_receive();
    }

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
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

/* Onboard ICM42688P IMU -- SPI1, CS=PC15, SCK=PA5, MISO=PA6, MOSI=PD7.
   See board.h's own comment on board_imu_spi_init() for the pin
   provenance and the board-owns-the-bus/lib-owns-the-protocol split.
   Blocking HAL_SPI_Transmit/Receive, not DMA/interrupt-driven -- this
   bus is only ever touched a handful of times per 20ms IMU task tick
   (lib/sensors/imu.c), nowhere near tight enough to need anything
   fancier, same reasoning board_sbus_uart_init() gives for why *that*
   peripheral needed DMA and this one doesn't. */

static SPI_HandleTypeDef imuSpi;

static void imu_spi_cs_low(void) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_RESET);
}

static void imu_spi_cs_high(void) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_SET);
}

void board_imu_spi_init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    /* SCK (PA5) + MISO (PA6) -- AF5, confirmed against real CubeMX-
       generated STM32H7 reference projects using this same PD7/PA5/PA6
       SPI1 pin trio (H750, same AF table as this board's H743). */
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    gpioInit.Mode = GPIO_MODE_AF_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
    gpioInit.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &gpioInit);

    /* MOSI (PD7) -- same AF5/SPI1, different port. */
    gpioInit.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOD, &gpioInit);

    /* CS (PC15) -- plain GPIO output, software-controlled: this chip's
       CS needs to frame each register transaction explicitly (held low
       for the address+data bytes, see board_imu_spi_write_reg()/
       board_imu_spi_read_regs() below), not SPI1's own hardware NSS. */
    gpioInit.Pin = GPIO_PIN_15;
    gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &gpioInit);
    imu_spi_cs_high(); /* idle high */

    /* SPI1's kernel clock defaults to PLL1Q (RCC_SPI123CLKSOURCE_PLL,
       the reset value of RCC_D2CCIP1R_SPI123SEL -- confirmed against
       this project's own copy of stm32h7xx_hal_rcc_ex.h; nothing in
       system_clock_config() overrides it, so this is the actual clock
       feeding SPI1 today). PLL1Q = VCO/PLLQ = 480MHz/2 = 240MHz on
       Rev.V silicon (60/1 N/M) or 400MHz/2 = 200MHz on older silicon
       (50/1 N/M) -- see system_clock_config()'s own PLL comment.
       BaudRatePrescaler /256 lands at ~940kHz/~781kHz respectively --
       this project's own arithmetic (not sourced from a working
       reference the way the register map below is), picked to land
       comfortably under the ICM42688P's real 24MHz SPI max for a first
       bring-up, same "conservative first bring-up clock" reasoning
       aoa-boat-controller's own ImuReader used for its (Arduino-
       library-derived) 1MHz choice. Bench-confirm the resulting SCK
       frequency with a scope/logic analyzer once flashed; raise once
       working. */
    imuSpi.Instance = SPI1;
    imuSpi.Init.Mode = SPI_MODE_MASTER;
    imuSpi.Init.Direction = SPI_DIRECTION_2LINES;
    imuSpi.Init.DataSize = SPI_DATASIZE_8BIT;
    imuSpi.Init.CLKPolarity = SPI_POLARITY_LOW;  /* Mode 0 -- ICM42688P requirement */
    imuSpi.Init.CLKPhase = SPI_PHASE_1EDGE;      /* Mode 0 */
    imuSpi.Init.NSS = SPI_NSS_SOFT;
    imuSpi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    imuSpi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    imuSpi.Init.TIMode = SPI_TIMODE_DISABLE;
    imuSpi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    imuSpi.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&imuSpi) != HAL_OK) {
        Error_Handler();
    }
}

void board_imu_spi_write_reg(uint8_t reg, uint8_t value) {
    uint8_t const txBuf[2] = {(uint8_t)(reg & 0x7F), value}; /* MSB clear = write */

    imu_spi_cs_low();
    HAL_SPI_Transmit(&imuSpi, (uint8_t *)txBuf, sizeof(txBuf), HAL_MAX_DELAY);
    imu_spi_cs_high();
}

void board_imu_spi_read_regs(uint8_t startReg, uint8_t *buf, uint8_t len) {
    uint8_t const addr = (uint8_t)(startReg | 0x80); /* MSB set = read */

    imu_spi_cs_low();
    HAL_SPI_Transmit(&imuSpi, (uint8_t *)&addr, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&imuSpi, buf, len, HAL_MAX_DELAY);
    imu_spi_cs_high();
}

/* Onboard I2C2 -- SCL=PB10, SDA=PB11 (AF4). See board.h's own comment for
   pin/bus provenance and the idempotent-init reasoning. This board's IMU
   is SPI-only, so this is a fresh peripheral bring-up, not a bus shared
   with anything else yet (unlike afroflight32's I2C2, shared with its
   IMU).

   TIMINGR (Fast Mode, 400kHz) is computed, not guessed -- H7's I2C
   peripheral takes a raw TIMINGR word (PRESC/SCLDEL/SDADEL/SCLH/SCLL),
   not a simple ClockSpeed field the way F1's I2C does (see
   boards/afroflight32/board.c's own I2C2 init for that simpler case).
   Ported the exact calculation from Betaflight's real, deployed
   drivers/bus_i2c_timing.c (i2cClockComputeRaw()/i2cClockTIMINGR(), the
   same RM0433-derived formula ST's own CubeMX timing tool uses) and ran
   it for this board's actual I2C2 kernel clock -- I2C123SEL defaults to
   D2PCLK1 (APB1, confirmed against this project's own copy of
   stm32h7xx_hal_rcc_ex.h: RCC_I2C123CLKSOURCE_D2PCLK1 is the reset
   value, so nothing in system_clock_config() needs to override it),
   which is 120MHz on Rev.V silicon (PCLK1 = HCLK/2 = 240MHz/2) or 100MHz
   on older silicon (200MHz/2) -- same isRevV split system_clock_config()
   already makes for the PLL. Results: 0x20F91940 (120MHz) /
   0x20C71435 (100MHz), both for 400kHz with no extra digital filter
   (dfcoeff=0, matching Betaflight's own AnalogFilter-only default). */
#define I2C2_TIMING_120MHZ_400KHZ 0x20F91940U
#define I2C2_TIMING_100MHZ_400KHZ 0x20C71435U

static I2C_HandleTypeDef baroI2c;
static bool baroI2cInitialized = false;

void board_i2c2_init(void) {
    if (baroI2cInitialized) {
        return;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();

    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    gpioInit.Mode = GPIO_MODE_AF_OD; /* open-drain -- wired-AND I2C bus */
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInit.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOB, &gpioInit);

    bool const isRevV = (HAL_GetREVID() == REV_ID_V);

    baroI2c.Instance = I2C2;
    baroI2c.Init.Timing = isRevV ? I2C2_TIMING_120MHZ_400KHZ : I2C2_TIMING_100MHZ_400KHZ;
    baroI2c.Init.OwnAddress1 = 0;
    baroI2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    baroI2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    baroI2c.Init.OwnAddress2 = 0;
    baroI2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    baroI2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    baroI2c.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&baroI2c) != HAL_OK) {
        Error_Handler();
    }

    baroI2cInitialized = true;
}

bool board_i2c2_write_reg(uint8_t devAddr, uint8_t reg, uint8_t value) {
    uint8_t const txBuf[2] = {reg, value};
    return HAL_I2C_Master_Transmit(&baroI2c, (uint16_t)(devAddr << 1), (uint8_t *)txBuf, sizeof(txBuf),
                                    HAL_MAX_DELAY) == HAL_OK;
}

bool board_i2c2_read_regs(uint8_t devAddr, uint8_t reg, uint8_t *buf, uint8_t len) {
    if (HAL_I2C_Master_Transmit(&baroI2c, (uint16_t)(devAddr << 1), &reg, 1, HAL_MAX_DELAY) != HAL_OK) {
        return false;
    }
    return HAL_I2C_Master_Receive(&baroI2c, (uint16_t)(devAddr << 1), buf, len, HAL_MAX_DELAY) == HAL_OK;
}

/* Battery voltage/current sense -- ADC1, PC0 (VBAT, ADC123_INP10) + PC1
   (CURR, ADC123_INP11) per the STM32H743 datasheet's own pinout table.
   See board.h's own comment on board_battery_adc_init() for the full
   pin/clock provenance (issue #24). */

static ADC_HandleTypeDef batteryAdc;

void board_battery_adc_init(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_ADC12_CLK_ENABLE();

    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpioInit.Mode = GPIO_MODE_ANALOG;
    gpioInit.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &gpioInit);

    /* CLKP (per_ck) defaults to HSI (~64MHz nominal) at reset --
       RCC_CLKPSOURCE_HSI is 0, CKPERSEL is never touched by
       system_clock_config() (only HSE/PLL1 for sysclk, HSI48 for USB),
       and HSI itself stays enabled through the HSE/PLL1 switch
       (HAL_RCC_OscConfig there never disables it) -- so selecting
       RCC_ADCCLKSOURCE_CLKP reaches a real, already-running clock with
       no PLL2/PLL3 configuration needed. */
    RCC_PeriphCLKInitTypeDef periphClkInit = {0};
    periphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    periphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInit) != HAL_OK) {
        Error_Handler();
    }

    batteryAdc.Instance = ADC1;
    /* ASYNC_DIV16 against ~64MHz per_ck -> ~4MHz ADC kernel clock -- see
       board.h's own comment on why this is deliberately conservative. */
    batteryAdc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV16;
    /* 12-bit, matching aoa-boat-controller's own analogReadResolution(12)
       -- board_battery_adc_read_*_raw()'s callers (lib/sensors/battery.c)
       reuse that project's already-bench-confirmed divider-scale math
       (issue #24's own body), which assumes this same denominator. */
    batteryAdc.Init.Resolution = ADC_RESOLUTION_12B;
    batteryAdc.Init.ScanConvMode = ADC_SCAN_DISABLE;
    batteryAdc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    batteryAdc.Init.LowPowerAutoWait = DISABLE;
    batteryAdc.Init.ContinuousConvMode = DISABLE;
    batteryAdc.Init.NbrOfConversion = 1;
    batteryAdc.Init.DiscontinuousConvMode = DISABLE;
    batteryAdc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    batteryAdc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    batteryAdc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    batteryAdc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    batteryAdc.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    batteryAdc.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&batteryAdc) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_ADCEx_Calibration_Start(&batteryAdc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }
}

/* Blocking, polled single-conversion read -- battery voltage/current are
   slow-changing (lib/sensors/battery.c's own task period), nowhere near
   tight enough to need DMA/interrupt handling, same reasoning
   board_imu_spi_init()'s own comment gives for its bus. A long sample
   time (387.5 ADC clock cycles, the second-longest this chip offers) is
   used deliberately -- no throughput pressure here, so there's no reason
   not to let an unbuffered resistor-divider input settle generously.
   The first conversion after a channel switch is discarded before the
   real read -- carried over defensively from aoa-boat-controller's own
   reference read (its own comment flags this as channel-switch/sample-
   hold settling under STM32duino's analogRead(); unconfirmed whether
   this raw-HAL path still needs it, but cheap enough to keep). */
static uint16_t battery_adc_read_channel(uint32_t channel) {
    ADC_ChannelConfTypeDef chanConfig = {0};
    chanConfig.Channel = channel;
    chanConfig.Rank = ADC_REGULAR_RANK_1;
    chanConfig.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;
    chanConfig.SingleDiff = ADC_SINGLE_ENDED;
    chanConfig.OffsetNumber = ADC_OFFSET_NONE;
    chanConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&batteryAdc, &chanConfig) != HAL_OK) {
        Error_Handler();
    }

    HAL_ADC_Start(&batteryAdc);
    HAL_ADC_PollForConversion(&batteryAdc, HAL_MAX_DELAY);
    (void)HAL_ADC_GetValue(&batteryAdc); /* discarded settling read, see above */
    HAL_ADC_Stop(&batteryAdc);

    HAL_ADC_Start(&batteryAdc);
    HAL_ADC_PollForConversion(&batteryAdc, HAL_MAX_DELAY);
    uint16_t const raw = (uint16_t)HAL_ADC_GetValue(&batteryAdc);
    HAL_ADC_Stop(&batteryAdc);

    return raw;
}

uint16_t board_battery_adc_read_vbat_raw(void) {
    return battery_adc_read_channel(ADC_CHANNEL_10); /* PC0 */
}

uint16_t board_battery_adc_read_curr_raw(void) {
    return battery_adc_read_channel(ADC_CHANNEL_11); /* PC1 */
}

/* GPS UART -- USART3, PD9. See board.h's own comment on
   board_gps_uart_init() for the pin/hot-plug provenance (issue #40).
   Ported the "no presence check, non-blocking drain" shape from
   aoa-boat-controller's own GpsReader, not the register-level driver
   itself (that project uses a HardwareSerial/Arduino-core UART, not
   applicable here). */

#define GPS_UART_RING_SIZE 128

static volatile uint8_t gpsRxBuf[GPS_UART_RING_SIZE];
static volatile uint8_t gpsRxHead;
static volatile uint8_t gpsRxTail;

void board_gps_uart_init(void) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    /* PD9 only (RX) -- PD8 (USART3_TX) left unconfigured, this module
       never transmits to the GPS module in this first pass. */
    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_9;
    gpioInit.Mode = GPIO_MODE_AF_PP;
    gpioInit.Pull = GPIO_PULLUP;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    gpioInit.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpioInit);

    UART_HandleTypeDef gpsUart = {0};
    gpsUart.Instance = USART3;
    gpsUart.Init.BaudRate = 115200; /* NMEA's near-universal default rate */
    gpsUart.Init.WordLength = UART_WORDLENGTH_8B;
    gpsUart.Init.StopBits = UART_STOPBITS_1;
    gpsUart.Init.Parity = UART_PARITY_NONE;
    gpsUart.Init.Mode = UART_MODE_RX;
    gpsUart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    gpsUart.Init.OverSampling = UART_OVERSAMPLING_16;
    gpsUart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    gpsUart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    /* HAL_UART_Init only, same reasoning board_sport_uart_init() gives
       for using it purely to reach a correctly-configured peripheral
       (real baud-rate math against the actual clock tree) before
       switching to direct register access below -- no
       HAL_UART_Receive_IT chaining, which would leave a real re-arm gap
       between one byte's callback returning and the next RXNE getting
       re-armed; this ISR (below) has no such gap since RXNE just stays
       permanently enabled. */
    if (HAL_UART_Init(&gpsUart) != HAL_OK) {
        Error_Handler();
    }

    /* Priority 5 -- this project's established floor for any ISR
       alongside FreeRTOS, same as every other peripheral ISR in this
       file. This one never touches a FreeRTOS API (unlike UART7's,
       which notifies a task) -- gps.c polls board_gps_uart_available()
       from task context instead, no ISR-to-task handoff needed at
       NMEA's low sentence rate. */
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    LL_USART_EnableIT_RXNE(USART3);
}

bool board_gps_uart_available(void) {
    return gpsRxHead != gpsRxTail;
}

uint8_t board_gps_uart_read_byte(void) {
    uint8_t const b = gpsRxBuf[gpsRxTail];
    gpsRxTail = (uint8_t)((gpsRxTail + 1) % GPS_UART_RING_SIZE);
    return b;
}

void USART3_IRQHandler(void) {
    if (LL_USART_IsActiveFlag_RXNE(USART3)) {
        uint8_t const b = (uint8_t)USART3->RDR;
        uint8_t const nextHead = (uint8_t)((gpsRxHead + 1) % GPS_UART_RING_SIZE);
        if (nextHead != gpsRxTail) {
            gpsRxBuf[gpsRxHead] = b;
            gpsRxHead = nextHead;
        }
        /* A full buffer drops the byte rather than overwriting unread
           data -- same choice board_sport_uart's own ring buffer makes;
           gps.c's task drains this every GPS_TASK_PERIOD_MS, comfortably
           faster than 128 bytes could fill at 115200 baud's realistic
           NMEA sentence rate (a full GGA+RMC pair is well under 128
           bytes). */
    }
}

void board_init(void) {
    system_clock_config();
    led_init();
}
