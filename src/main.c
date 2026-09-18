#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "board_features.h"
#include "heartbeat.h"
#include "supervisor.h"

#if defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32F7)
#include "stm32f7xx_hal.h"
#elif defined(STM32F1)
#include "stm32f1xx_hal.h"
#else
#error "Unknown MCU family -- add its HAL include here for this board."
#endif

#if HELM_FEATURE_CLI
#include "bootloader.h"
#include "cli.h"
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

/* FreeRTOS hook: called from a task's own stack if pvPortMalloc() can't
   satisfy an allocation. Halts rather than continuing on a heap that's
   already known to be exhausted. */
void vApplicationMallocFailedHook(void) {
    __disable_irq();
    for (;;) {
    }
}

/* FreeRTOS hook: called if a task's stack overflows (requires
   configCHECK_FOR_STACK_OVERFLOW). Halts rather than continuing with
   corrupted memory below the stack. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    __disable_irq();
    for (;;) {
    }
}

/* CMSIS SysTick interrupt handler. Always feeds HAL's own millisecond
   tick (HAL_GetTick() depends on it, e.g. for HAL_Delay() during
   board_init() before the scheduler exists), and additionally drives
   FreeRTOS's tick once the scheduler has actually started. */
void SysTick_Handler(void) {
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

/* Composition root shared by every board -- board-specific init lives in
   boards/<target>/board.c, not here. Real modules get started here, each
   gated on board_features.h's HELM_FEATURE_* flags, so a board that
   doesn't budget for a feature (see boards/afroflight32/board_features.h)
   simply never starts that module's task, rather than every module
   having its own scattered per-board #ifdefs. */
int main(void) {
#if HELM_FEATURE_CLI
    // Check whether the CLI's `dfu` command left a reboot-into-bootloader
    // request behind; if so, jump straight into it and never return.
    // Literal first statement -- before HAL_Init()/board_init() touch any
    // clock or peripheral, see bootloader_jump_if_requested()'s own comment.
    bootloader_jump_if_requested();
#endif

    // Bring up HAL's own tick/timebase, then this board's clock tree and
    // any other early peripheral init board.c owns.
    HAL_Init();
    board_init();

    // Start the module-liveness supervisor (issue #4) before any module
    // that will register with it (.docs/architecture/module-architecture.md's
    // "Fault isolation" section) -- unconditional, not gated behind a
    // HELM_FEATURE_* flag, same as the heartbeat below: every board gets
    // the safety net regardless of feature budget.
    supervisor_start();

#if HELM_FEATURE_CLI
    // Start the CLI console task (issue #1) -- USB CDC transport, `status`/
    // `dfu`/`help` commands.
    cli_start();
#endif

    // Start the heartbeat task to physically show the board is live. Useful
    // as a bring-up/debug signal independent of the CLI (which not every
    // board has yet).
    debug_heartbeat_start();
    vTaskStartScheduler();

    for (;;) {
    }
}
