#include "diag.h"

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "board_features.h"
#include "rx.h"
#include "mapping.h"
#include "control.h"
#include "servo.h"
#include "shell.h"
#include "telemetry.h"
#if HELM_HAS_SPORT_UART
#include "sport.h"
#endif
#include "imu.h"
#include "baro.h"

/* `pipeline`: prints the Input->Mapping->Control->Output->Servo chain's
   final stage output -- as of #36, real per-slot output mapping
   (passthrough+reverse+failsafe, or the Pitch control loop), not just
   plumbing-proof passthrough. Made observable on the bench without
   needing real RX hardware or a scope on a PWM pin.

   Loop bound is HELM_SERVO_COUNT (board_features.h), NOT RX_MAX_CHANNELS
   -- ServoFrame.servos[]/OutputFrame.servos[] are sized to the real
   per-board servo slot count since #36, generally fewer than the RX
   channel count every earlier stage stayed sized to. */
static void diag_pipeline(void) {
    ServoFrame frame;
    servo_get_latest(&frame);

    char line[160];
    int n = snprintf(line, sizeof(line), "pipeline: status=%s servos=[",
                      frame.status == RX_STATUS_OK ? "OK" : "FAILSAFE");
    for (int i = 0; i < HELM_SERVO_COUNT && n < (int)sizeof(line); i++) {
        n += snprintf(line + n, sizeof(line) - n, "%s%u", i == 0 ? "" : " ", frame.servos[i]);
    }
    snprintf(line + n, sizeof(line) - n, "]\r\n");
    shell_print(line);
}

/* `mapping`: dumps the mapping stage's own output (issue #34) -- the
   final `pipeline` dump above can't show this, since output.c still just
   copies ControlFrame's channels[] through unchanged (#36 hasn't landed),
   never touching pitchMode/pitchTarget at all. This is the only
   bench-visible way to confirm mapping.c's chosen channels are actually
   being interpreted, until #36 adds a real consumer. */
static const char *pitch_mode_name(PitchMode mode) {
    switch (mode) {
        case PITCH_MODE_OFF:
            return "OFF";
        case PITCH_MODE_LIMIT:
            return "LIMIT";
        case PITCH_MODE_ACTIVE:
            return "ACTIVE";
    }
    return "?";
}

static void diag_mapping(void) {
    MappingFrame frame;
    mapping_get_latest(&frame);

    char line[80];
    snprintf(line, sizeof(line), "mapping: status=%s pitch_mode=%-6s pitch_target=%u\r\n",
             frame.status == RX_STATUS_OK ? "OK" : "FAILSAFE", pitch_mode_name(frame.pitchMode),
             frame.pitchTarget);
    shell_print(line);
}

/* `control`: dumps the control stage's own output (issue #35) -- same
   reasoning as diag_mapping() above: output.c still just copies
   channels[] through unchanged (#36 hasn't landed), never touching
   pitchActive/pitchOutput, so this is the only bench-visible way to
   confirm the Off-mode/no-output and placeholder-passthrough branches
   both actually run. */
static void diag_control(void) {
    ControlFrame frame;
    control_get_latest(&frame);

    char line[80];
    snprintf(line, sizeof(line), "control: status=%s pitch_active=%s pitch_output=%u\r\n",
             frame.status == RX_STATUS_OK ? "OK" : "FAILSAFE", frame.pitchActive ? "yes" : "no",
             frame.pitchOutput);
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

/* No default case (issue #33) -- same "-Wswitch catches a forgotten
   field" reasoning as telemetry.c's own sensor_status_to_telemetry_status(),
   so adding a TelemetryField without a name here is a build warning, not
   a silent "?" in diag output. */
static const char *telemetry_field_name(TelemetryField field) {
    switch (field) {
        case TELEM_FIELD_TEST:
            return "test";
        case TELEM_FIELD_BARO_PRESSURE:
            return "baro_pa";
        case TELEM_FIELD_BARO_TEMPERATURE:
            return "baro_degc";
        case TELEM_FIELD_BATTERY_VOLTAGE:
            return "batt_v";
        case TELEM_FIELD_BATTERY_CURRENT:
            return "batt_a";
        case TELEM_FIELD_GPS_LATITUDE:
            return "gps_lat";
        case TELEM_FIELD_GPS_LONGITUDE:
            return "gps_lon";
        case TELEM_FIELD_GPS_ALTITUDE:
            return "gps_alt_m";
        case TELEM_FIELD_GPS_SPEED:
            return "gps_speed_mps";
        case TELEM_FIELD_GPS_SATELLITES:
            return "gps_sats";
        case TELEM_FIELD_COUNT:
            break;
    }
    return "?";
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

/* Generic to sensors.h's shared SensorStatus, not IMU-specific -- unlike
   diag_imu() below, this has no dependency on HELM_HAS_IMU (SensorStatus
   is defined unconditionally) and stays ungated so any future sensor's
   diag command (baro/#23, mag, gps, ...) can reuse it instead of
   duplicating it. */
static const char *sensor_status_name(SensorStatus status) {
    switch (status) {
        case SENSOR_STATUS_OK:
            return "OK";
        case SENSOR_STATUS_STALE:
            return "STALE";
        case SENSOR_STATUS_FAILED:
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

#if HELM_HAS_SPORT_UART
/* `sport`: poll-marker vs. poll-match counters (issue #18) -- see
   sport.h's own comment for why both, not just one "poll answered"
   count: tells "receiver isn't polling at all" apart from "polling, but
   our physical ID never matches" apart from "genuinely working". */
static void diag_sport(void) {
    uint32_t markers, matches;
    sport_get_counters(&markers, &matches);

    char line[64];
    snprintf(line, sizeof(line), "sport: poll_markers=%lu poll_matches=%lu\r\n", (unsigned long)markers,
             (unsigned long)matches);
    shell_print(line);
}
#endif

#if HELM_HAS_IMU
/* `imu`: dumps the IMU sample queue's current state (issue #14) -- proves
   imu_start()'s task/queue/supervisor wiring round-trips on real
   hardware even before #15 adds a real chip read. Gated on HELM_HAS_IMU
   -- the exact same condition main.c uses to decide whether imu_start()
   ever runs -- not for link-time reasons (imu.c has no whole-file guard
   like sport.c; it's unconditionally compiled, same as telemetry.c/
   rx.c), but because imu_get_latest() peeks imu_queue, which stays NULL
   if imu_start() was never called: configASSERT is a no-op in this
   project's FreeRTOSConfig.h, so xQueuePeek() on a NULL handle would
   fault, not gracefully no-op. Values printed milli-g/milli-deg-per-s
   scaled to an integer, same reason diag_telemetry() avoids %f. Always
   FAILED / all-zero until #15 lands. */
static void diag_imu(void) {
    ImuSample sample;
    imu_get_latest(&sample);

    char line[128];
    snprintf(line, sizeof(line),
             "imu: status=%-6s accel_mg=[%ld %ld %ld] gyro_mdps=[%ld %ld %ld]\r\n",
             sensor_status_name(sample.status), (long)(sample.accel_g[0] * 1000.0f),
             (long)(sample.accel_g[1] * 1000.0f), (long)(sample.accel_g[2] * 1000.0f),
             (long)(sample.gyro_dps[0] * 1000.0f), (long)(sample.gyro_dps[1] * 1000.0f),
             (long)(sample.gyro_dps[2] * 1000.0f));
    shell_print(line);
}
#endif

#if HELM_HAS_BARO
/* `baro`: dumps the baro sample queue's current state (issue #23), same
   shape/gating reasoning as diag_imu() above (baro_get_latest() peeks
   baro_queue, which is NULL if baro_start() was never called). Pressure
   printed as a whole Pascal (no scaling needed -- station pressure is
   ~100000 Pa, already meaningfully precise as an integer); temperature
   milli-deg-C scaled, same reason diag_imu()/diag_telemetry() avoid
   %f. */
static void diag_baro(void) {
    BaroSample sample;
    baro_get_latest(&sample);

    char line[96];
    snprintf(line, sizeof(line), "baro: status=%-6s pressure_pa=%ld temp_mdegc=%ld\r\n",
             sensor_status_name(sample.status), (long)sample.pressure_pa,
             (long)(sample.temperature_c * 1000.0f));
    shell_print(line);
}
#endif

void diag_dispatch(const char *args) {
    if (strcmp(args, "pipeline") == 0) {
        diag_pipeline();
    } else if (strcmp(args, "mapping") == 0) {
        diag_mapping();
    } else if (strcmp(args, "control") == 0) {
        diag_control();
    } else if (strcmp(args, "wedge") == 0) {
        diag_wedge();
    } else if (strcmp(args, "telemetry") == 0) {
        diag_telemetry();
#if HELM_HAS_SPORT_UART
    } else if (strcmp(args, "sport") == 0) {
        diag_sport();
#endif
#if HELM_HAS_IMU
    } else if (strcmp(args, "imu") == 0) {
        diag_imu();
#endif
#if HELM_HAS_BARO
    } else if (strcmp(args, "baro") == 0) {
        diag_baro();
#endif
    } else {
        shell_print("usage: diag <subcommand> -- available: pipeline, mapping, control, wedge, telemetry"
#if HELM_HAS_SPORT_UART
                    ", sport"
#endif
#if HELM_HAS_IMU
                    ", imu"
#endif
#if HELM_HAS_BARO
                    ", baro"
#endif
                    "\r\n");
    }
}
