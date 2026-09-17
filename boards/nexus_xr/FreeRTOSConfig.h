#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Placeholder only -- board.c #errors before this matters. STM32F722 is
   Cortex-M7 with a single-precision-only FPU (fpv5-sp-d16, unlike the
   H743's double-precision fpv5-d16) -- platformio.ini's per-env FPU flags
   need to reflect that once this target is real, not copy the H743's. */

#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ (0UL) /* TODO: unknown until HSE + PLL config is confirmed */
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 7
#define configMINIMAL_STACK_SIZE 128
#define configTOTAL_HEAP_SIZE (32 * 1024) /* TODO: placeholder, tune against real RAM budget */
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 4
#define configTIMER_QUEUE_LENGTH 10
#define configTIMER_TASK_STACK_DEPTH 256
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configQUEUE_REGISTRY_SIZE 8

#define configPRIO_BITS 4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 0xf
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define INCLUDE_vTaskPrioritySet 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelayUntil 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#endif /* FREERTOS_CONFIG_H */
