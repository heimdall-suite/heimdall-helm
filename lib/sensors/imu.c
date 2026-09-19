#include "imu.h"
#include "imu_chip.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"

/* 20ms period: comfortably slower than any of this project's real onboard
   IMU's own output data rate (icm42688p.c/mpu6500.c both configure
   ~1kHz ODR), matching the reasoning every other stage's stub period
   uses (see rx.c's own comment) -- module-architecture.md's priority
   tiers still aren't decided, so this is a placeholder value, not one
   picked against a designed control-loop rate yet. */
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

    /* One-time bring-up against whichever chip file this board's env
       selected (see imu_chip.h) -- a failed init is permanent for this
       boot, same as the previous plumbing-only stub always reporting
       FAILED, just now conditioned on a real identity check instead of
       unconditionally. */
    bool const chipReady = imu_chip_init();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(IMU_TASK_PERIOD_MS));

        ImuSample sample = {0};
        if (chipReady && imu_chip_read(sample.accel_g, sample.gyro_dps)) {
            sample.status = SENSOR_STATUS_OK;
        } else {
            /* Either init never succeeded, or this tick's read failed --
               never publish a stale/zeroed sample dressed up as OK. */
            sample.status = SENSOR_STATUS_FAILED;
        }

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
