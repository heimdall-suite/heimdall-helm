#include "diag.h"

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "rx.h"
#include "servo.h"
#include "shell.h"
#include "telemetry.h"

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
   resets the board on a wedge, not just in theory.

   Also bench-verified on afroflight32 (issue #11, 2026-09-19), over the
   CLI's UART/CP210x transport rather than USB CDC: `status` before and
   after showed uptime drop from 31241ms to 581ms, same signal. */
static void diag_wedge(void) {
    shell_print("wedging a task above supervisor priority -- IWDG should reset the board shortly...\r\n");
    xTaskCreate(wedge_task, "wedge", configMINIMAL_STACK_SIZE, NULL, configMAX_PRIORITIES - 1, NULL);
}

static const char *telemetry_field_name(TelemetryField field) {
    switch (field) {
        case TELEM_FIELD_TEST:
            return "test";
        default:
            return "?";
    }
}

static const char *telemetry_status_name(TelemetryStatus status) {
    switch (status) {
        case TELEM_STATUS_OK:
            return "OK";
        case TELEM_STATUS_STALE:
            return "STALE";
        case TELEM_STATUS_FAILED:
            return "FAILED";
        default:
            return "?";
    }
}

/* `telemetry`: dumps the telemetry table's current state (issue #16) --
   every field this build knows about, whether or not anything has ever
   written to it. Useful for #17's synthetic-value proof and beyond:
   confirms telemetry_set()/telemetry_get() round-trip on real hardware
   without needing a protocol adapter (#18/#19) wired up yet. Value
   printed truncated to an integer, not %f -- this toolchain's
   snprintf() float support isn't confirmed (see .agents/AGENTS.md's
   no-fabrication standard; nothing else in this codebase prints a float
   yet either), and every field is still a placeholder anyway. */
static void diag_telemetry(void) {
    for (int i = 0; i < TELEM_FIELD_COUNT; i++) {
        TelemetryEntry entry;
        telemetry_get((TelemetryField)i, &entry);

        uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
        char line[96];
        snprintf(line, sizeof(line), "telemetry: %-8s value=%ld status=%-6s age_ms=%lu\r\n",
                 telemetry_field_name((TelemetryField)i), (long)entry.value,
                 telemetry_status_name(entry.status),
                 (unsigned long)(now_ms - entry.last_updated_ms));
        shell_print(line);
    }
}

void diag_dispatch(const char *args) {
    if (strcmp(args, "pipeline") == 0) {
        diag_pipeline();
    } else if (strcmp(args, "wedge") == 0) {
        diag_wedge();
    } else if (strcmp(args, "telemetry") == 0) {
        diag_telemetry();
    } else {
        shell_print("usage: diag <subcommand> -- available: pipeline, wedge, telemetry\r\n");
    }
}
