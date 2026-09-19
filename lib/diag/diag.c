#include "diag.h"

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "rx.h"
#include "servo.h"
#include "shell.h"

/* `pipeline`: prints the Input->Mapping->Control->Output->Servo stub
   chain's (issue #7) final stage output -- the plumbing this chain
   exists to prove, made observable on the bench without needing real RX
   hardware or a scope on a PWM pin. */
static void diag_pipeline(void) {
    ServoFrame frame;
    servo_get_latest(&frame);

    char line[160];
    int n = snprintf(line, sizeof(line), "pipeline: status=%s servos=[",
                      frame.status == RX_STATUS_OK ? "OK" : "FAILSAFE");
    for (int i = 0; i < RX_MAX_CHANNELS && n < (int)sizeof(line); i++) {
        n += snprintf(line + n, sizeof(line) - n, "%s%u", i == 0 ? "" : " ", frame.servos[i]);
    }
    snprintf(line + n, sizeof(line) - n, "]\r\n");
    shell_print(line);
}

/* Deliberately never yields or blocks -- see diag_wedge()'s comment
   below for what this proves. */
static void wedge_task(void *arg) {
    (void)arg;
    for (;;) {
    }
}

/* `wedge`: created at the highest FreeRTOS priority so it starves every
   other task in the system, including the supervisor (priority 2, see
   lib/supervisor/supervisor.c), of any CPU time at all -- the "wedged
   task that stops the supervisor's pass" failure mode
   .docs/architecture/module-architecture.md's "Crash safety" section
   describes, distinct from vApplicationStackOverflowHook/
   vApplicationMallocFailedHook in src/main.c, which halt by disabling
   interrupts directly instead. Once this runs, the supervisor can never
   feed IWDG again, and the chip resets on its own shortly after -- there
   is no way back from this short of that reset.

   Bench-verified on matek_h743 (issue #5, 2026-09-19): running this
   dropped the board's USB CDC enumeration for ~200ms and it came back
   with a fresh (~1s) uptime, confirming the IWDG backstop actually
   resets the board on a wedge, not just in theory. */
static void diag_wedge(void) {
    shell_print("wedging a task above supervisor priority -- IWDG should reset the board shortly...\r\n");
    xTaskCreate(wedge_task, "wedge", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
}

void diag_dispatch(const char *args) {
    if (strcmp(args, "pipeline") == 0) {
        diag_pipeline();
    } else if (strcmp(args, "wedge") == 0) {
        diag_wedge();
    } else {
        shell_print("usage: diag <subcommand> -- available: pipeline, wedge\r\n");
    }
}
