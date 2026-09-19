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
}

/* PB4 "CAL" LED -- see board_led_toggle()'s own comment in board.h for
   the pin/polarity source. Freed from NJTRST by system_clock_config()'s
   AFIO SWJ-NOJTAG remap above, which already has to run before this. */
static void led_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpioInit = {0};
    gpioInit.Pin = GPIO_PIN_4;
    gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpioInit);

    /* active-low: start off */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
}

void board_led_toggle(void) {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4);
}

void board_init(void) {
    system_clock_config();
    led_init();
}
