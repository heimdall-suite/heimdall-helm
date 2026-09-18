#include "servo.h"

#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"
#include "output.h"

/* See mapping.c's own comment -- same placeholder-period-and-priority
   reasoning applies to every stage in this chain (issue #7). */
#define SERVO_TASK_PERIOD_MS 20
#define SERVO_TASK_PRIORITY 1

static QueueHandle_t servo_queue;

static void servo_task(void *arg) {
    (void)arg;

    ServoFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    SupervisorHandle handle = supervisor_register("servo", servo_queue, &fallback,
                                                   sizeof(fallback),
                                                   pdMS_TO_TICKS(SERVO_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(SERVO_TASK_PERIOD_MS));

        OutputFrame in;
        output_get_latest(&in);

        /* "Driving" the servo is just recording the final values here --
           see servo.h. Real PWM output is a later issue. */
        ServoFrame out;
        memcpy(out.servos, in.servos, sizeof(out.servos));
        out.status = in.status;

        xQueueOverwrite(servo_queue, &out);
        supervisor_kick(handle);
    }
}

void servo_start(void) {
    servo_queue = xQueueCreate(1, sizeof(ServoFrame));

    ServoFrame initial = {0};
    initial.status = RX_STATUS_FAILSAFE;
    xQueueOverwrite(servo_queue, &initial);

    xTaskCreate(servo_task, "servo", configMINIMAL_STACK_SIZE, NULL,
                SERVO_TASK_PRIORITY, NULL);
}

void servo_get_latest(ServoFrame *out) {
    xQueuePeek(servo_queue, out, 0);
}
