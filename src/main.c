#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "board_features.h"

#if defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32F7)
#include "stm32f7xx_hal.h"
#elif defined(STM32F1)
#include "stm32f1xx_hal.h"
#else
#error "Unknown MCU family -- add its HAL include here for this board."
#endif

extern void xPortSysTickHandler(void);

/* Called by boards/<target>/board.c on an unrecoverable HAL clock/init
   failure -- before the scheduler exists, so there's no task to fail
   gracefully into. Halts rather than continuing on unconfirmed clocks. */
void Error_Handler(void) {
    __disable_irq();
    for (;;) {
    }
}

/* Bring-up milestone only: proves the toolchain (PlatformIO + framework=
   stm32cube + vendored FreeRTOS + per-board wiring) builds and links for
   this target. No real modules yet -- see repo root README's Status
   section. This file is the single composition root shared by every
   board; board-specific init lives in boards/<target>/board.c, not here.

   Once real modules exist, they get registered here, gated on
   board_features.h's HELM_FEATURE_* flags, e.g.:

       #if HELM_FEATURE_BLACKBOX
           blackbox_module_start();
       #endif

   -- so a board that doesn't budget for a feature (see
   boards/afroflight32/board_features.h) simply never creates that task,
   rather than every module having its own scattered per-board #ifdefs. */
static void heartbeat_task(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vApplicationMallocFailedHook(void) {
    __disable_irq();
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    __disable_irq();
    for (;;) {
    }
}

void SysTick_Handler(void) {
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

int main(void) {
    HAL_Init();
    board_init();

    xTaskCreate(heartbeat_task, "heartbeat", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    vTaskStartScheduler();

    for (;;) {
    }
}
