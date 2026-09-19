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

/* IWDG -- issue #11, porting #5's matek_h743 (STM32H7) IWDG to this
   board. Register-compatible in shape with the H7 (KR/PR/RLR,
   HAL_IWDG_Init/HAL_IWDG_Refresh identical API), confirmed directly
   against RM0008's own IWDG chapter rather than assumed identical to
   H7's RM0433: same PR prescaler range (/4 through /256) and same
   12-bit RLR reload (0-0xFFF, stm32f1xx_hal_iwdg.h's IS_IWDG_RELOAD), so
   the *shape* of this calculation matches board_iwdg_init() in
   boards/matek_h743/board.c. Unlike H7's IWDG_InitTypeDef, F1's has no
   Window member -- window mode doesn't exist on this IWDG, not an
   omission here.

   What does NOT carry over from H7: this chip's LSI runs at a nominal
   40kHz (LSI_VALUE, stm32f1xx_hal_conf.h / stm32f1xx_ll_rcc.h), not the
   H7's 32kHz -- so the same /32 prescaler gives a 0.8ms tick here, not
   H7's clean 1ms. Reload 311 (312 ticks) x 0.8ms = 249.6ms nominal
   timeout, matching H7's own ~250ms target (roughly 25x
   SUPERVISOR_POLL_PERIOD_MS's 10ms supervisor sweep, well under a
   second) as closely as this LSI/prescaler combination allows -- RM0008
   gives LSI a wide tolerance across temperature same as RM0433 does for
   H7, so this isn't meaningfully less precise than the number it's
   matching, just not as arithmetically clean. Like H7's IWDG1,
   HAL_IWDG_Init's __HAL_IWDG_START forces LSI on by itself -- no RCC LSI
   setup needed here either.

   Bench-verified on this exact board (issue #11, 2026-09-19), resolving
   the open verification question the issue started with: `diag wedge`
   over the CLI (reachable here per #12/#13) starved the supervisor as
   expected, and `status` afterward showed uptime had dropped from
   31241ms to 581ms -- a fresh boot, confirming the IWDG actually reset
   the board and not just compiled. */
static IWDG_HandleTypeDef iwdg;

void board_iwdg_init(void) {
    iwdg.Instance = IWDG;
    iwdg.Init.Prescaler = IWDG_PRESCALER_32;
    iwdg.Init.Reload = 311;
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
