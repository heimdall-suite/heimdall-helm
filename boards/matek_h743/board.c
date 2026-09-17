#include "board.h"
#include "stm32h7xx_hal.h"
#include <stdbool.h>

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

void board_init(void) {
    system_clock_config();
}
