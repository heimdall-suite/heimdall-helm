#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* configCPU_CLOCK_HZ assumes REV_ID_V silicon (480MHz path) -- see
   board.c's SystemClock_Config() for the silicon-revision check this
   depends on. The bench unit this was built against confirmed REV_ID_V
   (480MHz, see aoa-boat-controller's decisions.md). If a differently-
   provisioned unit ever lands on the 400MHz fallback path instead, this
   constant goes stale and FreeRTOS's tick timing is off by the 480/400
   ratio (~17% slow) until this is made dynamic (e.g. computed from
   HAL_RCC_GetSysClockFreq() at runtime instead of a compile-time
   constant). Not yet a problem for the current bench unit. */
#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ (480000000UL)
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 7
#define configMINIMAL_STACK_SIZE 128
#define configTOTAL_HEAP_SIZE (64 * 1024)
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

/* Cortex-M7 port handlers: renaming FreeRTOS's own function names to the
   CMSIS vector table's weak symbol names, so port.c's definitions satisfy
   the vector table directly. SysTick_Handler is NOT renamed here — this
   project provides its own (src/main.c), which calls HAL_IncTick() and
   xPortSysTickHandler() in sequence; see that file for why. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#endif /* FREERTOS_CONFIG_H */
