#include "heartbeat.h"

#include "board_features.h"
#include "FreeRTOS.h"
#include "task.h"

#if HELM_HAS_DEBUG_LED
#include "board.h"
#endif

/* Task body: wakes once a second forever, toggling the debug LED if this
   board has one (HELM_HAS_DEBUG_LED). Never returns. */
static void heartbeat_task(void *arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
#if HELM_HAS_DEBUG_LED
        board_led_toggle();
#endif
    }
}

void debug_heartbeat_start(void) {
    xTaskCreate(heartbeat_task, "heartbeat", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
}
