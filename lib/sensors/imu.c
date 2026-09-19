#include "imu.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"

/* Placeholder task period/priority -- pure plumbing stub (issue #14),
   same reasoning as every other stage's stub period (see rx.c's own
   comment): no real chip decode yet to time against (issue #15 picks
   real numbers once the IMU's actual output data rate is known), and
   module-architecture.md's priority tiers aren't decided. */
#define IMU_TASK_PERIOD_MS 20
#define IMU_TASK_PRIORITY 1

static QueueHandle_t imu_queue;

static void imu_task(void *arg) {
    (void)arg;

    /* Registers from this task's own context, not imu_start() -- see
       supervisor_register()'s header comment: concurrent module tasks
       registering after the scheduler starts is exactly the race its
       critical section protects against. */
    ImuSample fallback = {0};
    fallback.status = SENSOR_STATUS_FAILED;
    SupervisorHandle handle = supervisor_register("imu", imu_queue, &fallback, sizeof(fallback),
                                                   pdMS_TO_TICKS(IMU_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(IMU_TASK_PERIOD_MS));

        /* No real chip decode yet -- see imu.h's header comment. Always
           FAILED, never a stale zeroed sample dressed up as OK. */
        ImuSample sample = {0};
        sample.status = SENSOR_STATUS_FAILED;

        xQueueOverwrite(imu_queue, &sample);
        supervisor_kick(handle);
    }
}

void imu_start(void) {
    imu_queue = xQueueCreate(1, sizeof(ImuSample));

    /* Seed the queue before anything downstream can peek it -- a
       length-1 overwrite queue only holds a valid value once something
       has actually written to it once. */
    ImuSample initial = {0};
    initial.status = SENSOR_STATUS_FAILED;
    xQueueOverwrite(imu_queue, &initial);

    xTaskCreate(imu_task, "imu", configMINIMAL_STACK_SIZE, NULL, IMU_TASK_PRIORITY, NULL);
}

void imu_get_latest(ImuSample *out) {
    xQueuePeek(imu_queue, out, 0);
}
