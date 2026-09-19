#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION 1
#define configCPU_CLOCK_HZ (72000000UL)
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 7
#define configMINIMAL_STACK_SIZE 96
/* Bench-measured on real hardware (issue #13's bring-up): 6KB was too
   small -- the previous guess here undercounted vTaskStartScheduler()'s
   own internal task creation (IDLE + timer-service, on top of every
   module's own xTaskCreate()/xQueueCreate() from src/main.c). Confirmed
   via LED checkpoint bisection: all 8 application tasks + their queues
   created successfully (supervisor/rx/mapping/control/output/servo/cli/
   heartbeat), then vTaskStartScheduler() itself hit
   vApplicationMallocFailedHook() creating its own IDLE task -- the heap
   was exhausted by ~1KB at exactly that point, not before. 10KB leaves
   comfortable margin (this board still has 20KB RAM total; static
   usage elsewhere is under 2KB) without treating this the way the
   removed comment did ("deliberately small... TODO tune once measured")
   -- it's now actually measured, not a guess. */
#define configTOTAL_HEAP_SIZE (10 * 1024)
#define configMAX_TASK_NAME_LEN 12
#define configUSE_16_BIT_TICKS 0
#define configUSE_MUTEXES 1
#define configUSE_RECURSIVE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY 4
#define configTIMER_QUEUE_LENGTH 5
#define configTIMER_TASK_STACK_DEPTH 128
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configUSE_MALLOC_FAILED_HOOK 1
#define configQUEUE_REGISTRY_SIZE 4

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

/* Cortex-M3 port handlers: same renaming trick as the H743 config -- see
   that file's comment. SysTick_Handler is NOT renamed; src/main.c
   provides it directly. */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler

#endif /* FREERTOS_CONFIG_H */
