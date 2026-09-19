#include "baro.h"
#include "baro_chip.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"

/* 1000ms period -- matches both real chip drivers' own configured
   continuous-conversion cadence (dps310.c: 32Hz hardware rate, read
   throttled to 1/s by this task's own period rather than duplicating a
   throttle inside the chip file; bmp280.c: 1s standby time between
   normal-mode conversions) and both reference drivers' (aoa-boat-
   controller's BaroReader) own read-interval choice. Pressure/
   temperature change slowly -- there is no reason to poll faster than
   the chip itself actually produces new samples. */
#define BARO_TASK_PERIOD_MS 1000
#define BARO_TASK_PRIORITY 1

static QueueHandle_t baro_queue;

static void baro_task(void *arg) {
    (void)arg;

    /* Registers from this task's own context, not baro_start() -- see
       supervisor_register()'s header comment, same reasoning imu.c's
       own task uses. */
    BaroSample fallback = {0};
    fallback.status = SENSOR_STATUS_FAILED;
    SupervisorHandle handle = supervisor_register("baro", baro_queue, &fallback, sizeof(fallback),
                                                   pdMS_TO_TICKS(BARO_TASK_PERIOD_MS * 3));

    /* One-time bring-up against whichever chip file this board's env
       selected (see baro_chip.h) -- a failed init is permanent for this
       boot, same as imu.c's own chipReady handling. */
    bool const chipReady = baro_chip_init();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(BARO_TASK_PERIOD_MS));

        BaroSample sample = {0};
        if (chipReady && baro_chip_read(&sample.pressure_pa, &sample.temperature_c)) {
            sample.status = SENSOR_STATUS_OK;
        } else {
            /* Either init never succeeded, or this tick's read failed --
               never publish a stale sample as OK. */
            sample.status = SENSOR_STATUS_FAILED;
        }

        xQueueOverwrite(baro_queue, &sample);
        supervisor_kick(handle);
    }
}

void baro_start(void) {
    baro_queue = xQueueCreate(1, sizeof(BaroSample));

    /* Seed the queue before anything downstream can peek it -- a
       length-1 overwrite queue only holds a valid value once something
       has actually written to it once. */
    BaroSample initial = {0};
    initial.status = SENSOR_STATUS_FAILED;
    xQueueOverwrite(baro_queue, &initial);

    xTaskCreate(baro_task, "baro", configMINIMAL_STACK_SIZE, NULL, BARO_TASK_PRIORITY, NULL);
}

void baro_get_latest(BaroSample *out) {
    xQueuePeek(baro_queue, out, 0);
}
