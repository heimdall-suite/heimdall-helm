#include "FreeRTOS.h"
#include "task.h"
#include "stm32h7xx_hal.h"

extern void xPortSysTickHandler(void);

/* Bring-up milestone only: proves the toolchain (PlatformIO + framework=
   stm32cube + vendored FreeRTOS) builds and links for the Matek H743-WLITE.
   No clock config, drivers, or scheduler/module architecture yet — see
   repo root README's Status section. */
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

    xTaskCreate(heartbeat_task, "heartbeat", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    vTaskStartScheduler();

    for (;;) {
    }
}
