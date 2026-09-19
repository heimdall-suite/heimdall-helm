#include "board.h"
#include "stm32f1xx_hal.h"

/* Clock tree: 12MHz HSE x6 PLL = 72MHz sysclk, APB1 = HCLK/2 (36MHz,
   within the F103's 36MHz APB1 limit), APB2 = HCLK/1 (72MHz).

   Ported from aoa-boat-controller's src/Naze32/clock_config.cpp. That
   file's own comments carry the full provenance -- summarized here: the
   12MHz HSE crystal is owner-confirmed (not datasheet-assumed) against
   this exact physical board, matching how the original Cleanflight
   firmware configures it. genericSTM32F103CB's default clock config runs
   off the internal 8MHz HSI instead and never touches the external
   crystal at all -- this override is what makes the real 12MHz HSE
   actually used, which matters for anything timing-sensitive (UART baud
   rates in particular). */
static void system_clock_config(void) {
    /* PB4/PA15 double as NJTRST/JTDI and stay claimed by the debug port
       at reset regardless of GPIO config, until AFIO_MAPR's SWJ_CFG bits
       release them. This remap (JTAG off, SW-DP kept, 2-wire SWD) is what
       makes those pins usable as plain GPIO. */
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    RCC_OscInitTypeDef oscInit = {0};
    oscInit.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscInit.HSEState = RCC_HSE_ON;
    oscInit.PLL.PLLState = RCC_PLL_ON;
    oscInit.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscInit.PLL.PLLMUL = RCC_PLL_MUL6; /* 12MHz x 6 = 72MHz */
    if (HAL_RCC_OscConfig(&oscInit) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitTypeDef clkInit = {0};
    clkInit.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2;
    clkInit.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clkInit.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clkInit.APB1CLKDivider = RCC_HCLK_DIV2;
    clkInit.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clkInit, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }

    /* F103's "USB" peripheral needs exactly 48MHz, derived from the PLL
       via a fixed /1 or /1.5 prescaler (no other ratios exist) -- 72MHz
       PLL / 1.5 = 48MHz exactly, the standard combination for this
       family (RCC_CFGR's USBPRE bit resets to this same /1.5 setting by
       default, but set it explicitly rather than rely on that reset
       value matching -- same reasoning as every other clock field this
       function sets explicitly instead of leaving implicit). Needed for
       #12's USB CDC transport; harmless before that lands, since nothing
       enables the USB peripheral itself until usb_cdc_init() runs. */
    RCC_PeriphCLKInitTypeDef periphClkInit = {0};
    periphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    periphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
    if (HAL_RCCEx_PeriphCLKConfig(&periphClkInit) != HAL_OK) {
        Error_Handler();
    }
}

void board_init(void) {
    system_clock_config();
}
