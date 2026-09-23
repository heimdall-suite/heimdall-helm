#include "gps.h"

#include "board_features.h"

/* Whole-file guard, same idiom lib/telemetry/sport.c uses for
   HELM_HAS_SPORT_UART -- compiles to an empty translation unit on any
   board without a ported board_gps_uart_*() transport (board.h). Issue
   #56 retired the old HELM_HAS_GPS flag this used to guard on -- see
   HELM_HAS_GPS_UART_TRANSPORT's own comment (board_features.h) for why. */
#if HELM_HAS_GPS_UART_TRANSPORT

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"
#include "board.h"
#if HELM_FEATURE_PARAMS_PERSIST
#include "params.h"
#endif

#define GPS_TASK_PERIOD_MS 100 /* faster than baro.c's 1000ms -- needs to drain
                                   board.h's ring buffer often enough that it
                                   never overflows at 115200 baud's realistic
                                   NMEA sentence rate (a GGA+RMC pair well under
                                   its 128-byte size), not because a fix itself
                                   updates this fast */
#define GPS_TASK_PRIORITY 1

/* NMEA sentences are <=82 chars per spec (NMEA 0183 4.10) -- some margin
   for modules that exceed it slightly. */
#define GPS_LINE_MAX_LEN 96
#define GPS_MAX_FIELDS 20

static QueueHandle_t gps_queue;
static GpsFix latestFix;

static char lineBuf[GPS_LINE_MAX_LEN];
static uint8_t lineLen;

/* Bench-diagnostic counters -- see gps_get_counters()'s own comment in
   gps.h for what these prove and why. Plain non-atomic increments: both
   only ever get written from gps_task's own context (gps_process_byte()/
   gps_process_sentence() are only ever called from there), and read
   from the CLI task via gps_get_counters() -- same benign single-writer/
   occasional-reader shape sport.c's own poll counters already rely on,
   a torn read here is at worst one stale/half-updated diagnostic number,
   never a control-relevant value. */
static uint32_t bytesReceivedCount;
static uint32_t validSentenceCount;

/* Parses NMEA's ddmm.mmmm (lat) / dddmm.mmmm (lon) format + N/S/E/W
   direction into signed decimal degrees. Integer truncation instead of
   floor() -- raw is always >= 0 here (NMEA encodes sign as a separate
   direction letter, never a leading '-'), so truncation toward zero is
   already floor() for this input, and this avoids pulling in <math.h>/
   libm for one call site. */
static float nmea_to_decimal_degrees(const char *field, char dir) {
    double const raw = strtod(field, NULL);
    double const degreesWhole = (double)(long)(raw / 100.0);
    double const minutes = raw - degreesWhole * 100.0;
    double decimal = degreesWhole + minutes / 60.0;
    if (dir == 'S' || dir == 'W') {
        decimal = -decimal;
    }
    return (float)decimal;
}

/* Additive XOR checksum between '$' and '*', matching every other
   checksum in this codebase's own "validate before trusting a frame"
   discipline (lib/telemetry/sport.c's own FrSky checksum, a different
   algorithm but the same principle). Corrupted/torn lines are dropped
   here, never partially parsed. */
static bool nmea_checksum_valid(const char *sentence) {
    const char *p = sentence + 1; /* skip leading '$' */
    uint8_t checksum = 0;
    while (*p != '\0' && *p != '*') {
        checksum ^= (uint8_t)*p;
        p++;
    }
    if (*p != '*') {
        return false; /* no checksum present -- torn line */
    }
    p++;
    uint8_t const expected = (uint8_t)strtoul(p, NULL, 16);
    return checksum == expected;
}

/* Splits `sentence` in place on ',' and '*' (NUL-stuffing each
   delimiter), same shape a hand-rolled CSV splitter needs -- NOT
   strtok(), which silently merges adjacent delimiters and would
   misalign every field after a NMEA sentence's very common empty field
   (",,", a sensor with nothing to report for that slot). Returns the
   number of fields found, capped at maxFields. */
static uint8_t split_fields(char *sentence, char *fields[], uint8_t maxFields) {
    uint8_t count = 0;
    char *p = sentence;
    fields[count++] = p;
    while (*p != '\0' && count < maxFields) {
        if (*p == ',' || *p == '*') {
            *p = '\0';
            fields[count++] = p + 1;
        }
        p++;
    }
    return count;
}

/* GGA: 0=$xxGGA,1=time,2=lat,3=N/S,4=lon,5=E/W,6=fixQuality,
   7=numSatellites,8=HDOP,9=altitude,10=altitude-units,... -- the
   authoritative source for latestFix.status: fixQuality 0 (or an empty
   lat/lon field -- a module can emit GGA before it has resolved a
   position at all) means FAILED, never a stale/zeroed OK. */
static void parse_gga(char *fields[]) {
    uint32_t const fixQuality = strtoul(fields[6], NULL, 10);
    if (fixQuality == 0 || fields[2][0] == '\0' || fields[4][0] == '\0') {
        latestFix.status = SENSOR_STATUS_FAILED;
        return;
    }

    latestFix.latitude_deg = nmea_to_decimal_degrees(fields[2], fields[3][0]);
    latestFix.longitude_deg = nmea_to_decimal_degrees(fields[4], fields[5][0]);
    latestFix.satellites = (uint8_t)strtoul(fields[7], NULL, 10);
    latestFix.altitude_m = (float)strtod(fields[9], NULL);
    latestFix.status = SENSOR_STATUS_OK;
}

/* RMC: 0=$xxRMC,1=time,2=status(A=valid/V=void),3=lat,4=N/S,5=lon,
   6=E/W,7=speed(knots),8=course,9=date,... -- only ever contributes
   speed_mps on top of whatever GGA already established; it does NOT set
   latestFix.status itself (issue #40's own scoping: GGA owns fix
   quality/position, RMC owns speed/course/fix-validity, and this driver
   treats GGA as the authoritative one for "is there a fix at all" since
   it's the sentence that actually carries position). A void ('V') RMC
   just leaves speed_mps untouched from whatever it last validly was. */
static void parse_rmc(char *fields[]) {
    if (fields[2][0] != 'A') {
        return;
    }
    double const speedKnots = strtod(fields[7], NULL);
    latestFix.speed_mps = (float)(speedKnots * 0.514444); /* 1 knot = 0.514444 m/s */
}

static void gps_process_sentence(char *sentence) {
    if (!nmea_checksum_valid(sentence)) {
        return;
    }
    validSentenceCount++;

    char *fields[GPS_MAX_FIELDS];
    uint8_t const fieldCount = split_fields(sentence, fields, GPS_MAX_FIELDS);

    /* fields[0] is "$GPGGA"/"$GNGGA"/"$GLGGA"/etc -- match the last 3
       characters (the sentence type) regardless of talker ID, since
       that varies by module/constellation and isn't this driver's
       concern (same defensive convention most NMEA parsers use). */
    size_t const idLen = strlen(fields[0]);
    if (idLen < 3) {
        return;
    }
    char const *type = fields[0] + idLen - 3;

    if (strcmp(type, "GGA") == 0 && fieldCount >= 10) {
        parse_gga(fields);
    } else if (strcmp(type, "RMC") == 0 && fieldCount >= 9) {
        parse_rmc(fields);
    }
    /* Every other sentence type: checksum-verified above, but discarded
       here -- same "GGA/RMC only, every other sentence just proves the
       line parsed cleanly" scope aoa-boat-controller's own GpsReader
       uses (issue #40's own body). */
}

/* Accumulates one byte into lineBuf, dispatching a complete sentence to
   gps_process_sentence() on a line terminator. '$' always resets to a
   fresh line (a NMEA module never nests sentences), so this recovers
   automatically from a byte dropped by board.h's ring buffer (issue
   #40's own board.c comment on that being possible under a full buffer)
   or from having been powered on mid-sentence -- no separate resync
   state machine needed, same self-resynchronizing reasoning
   board_sbus_uart_take_frame()'s own header comment gives for SBUS's
   idle-line capture. */
static void gps_process_byte(uint8_t b) {
    bytesReceivedCount++;

    if (b == '$') {
        lineLen = 0;
        lineBuf[lineLen++] = (char)b;
        return;
    }

    if (lineLen == 0) {
        return; /* mid-stream, before the first '$' this boot (or after a drop) */
    }

    if (b == '\r' || b == '\n') {
        lineBuf[lineLen] = '\0';
        gps_process_sentence(lineBuf);
        lineLen = 0;
        return;
    }

    if (lineLen < GPS_LINE_MAX_LEN - 1) {
        lineBuf[lineLen++] = (char)b;
    } else {
        /* Overlong line -- drop it, resync on the next '$' rather than
           parsing a truncated/misaligned sentence. */
        lineLen = 0;
    }
}

/* Whether board_gps_uart_init() actually bound a real UART this boot --
   an unassigned port, an unresolvable source, or an unsupported
   protocol all leave this false, same "never even try to read hardware"
   treatment (#56's own scope note: unassigned port and unplugged module
   collapse into the same case). Set once, in gps_task(), never touched
   again -- same "bind once, forward every tick" shape rx_start()'s own
   driver pick already established; changing gps.port/gps.source live
   would need a reboot to actually take effect regardless, since it's a
   hardware rebind, not a plain calibration number. */
static bool gps_uart_bound;

static void gps_task(void *arg) {
    (void)arg;

    GpsFix fallback = {0};
    fallback.status = SENSOR_STATUS_FAILED;
    SupervisorHandle handle = supervisor_register("gps", gps_queue, &fallback, sizeof(fallback),
                                                    pdMS_TO_TICKS(GPS_TASK_PERIOD_MS * 3));

#if HELM_FEATURE_PARAMS_PERSIST
    /* Resolve gps.source/gps.port/gps.protocol (#54) once, here.
       PARAM_PORT_SOURCE_DIRECT is the only source this issue implements
       -- bridge-named values are accepted by param_set_port()'s own
       validation for forward compatibility (#54's reserved value shape)
       but never acted on here, since no bridge subsystem exists yet
       (#56's own explicit out-of-scope note); anything else -- NONE,
       ONBOARD (GPS has no onboard case, unlike mag), or a not-yet-real
       bridge value -- leaves gps_uart_bound false. Same treatment for
       GPS_PROTOCOL_NMEA: it's the only decoder this file implements, so
       any other value (a future UBX pick, #56's own "out of scope,
       natural follow-up" note) also just never binds -- an unsupported
       protocol has nothing valid to read, same as no module attached. */
    uint32_t source = PARAM_PORT_SOURCE_NONE;
    uint32_t port = PARAM_PORT_UNSET;
    uint32_t protocol = GPS_PROTOCOL_NMEA;
    param_get_u32(PARAM_GPS_SOURCE, &source);
    param_get_u32(PARAM_GPS_PORT, &port);
    param_get_u32(PARAM_GPS_PROTOCOL, &protocol);

    if (source == PARAM_PORT_SOURCE_DIRECT && protocol == GPS_PROTOCOL_NMEA) {
        /* board_gps_uart_init() itself returns false (nothing
           initialized/armed) for a port this board doesn't know how to
           bind GPS to -- gps.port c is this board's boot-identical
           default (params.c), b is the other real candidate (#56's own
           "worth confirming in testing" call-out); anything else
           degrades to the same unbound case, never a crash. */
        gps_uart_bound = board_gps_uart_init((uint8_t)port);
    }
#endif
    /* Without a persisted-param store there's no way to resolve a real
       port/source choice at all -- gps_uart_bound stays false, same
       FAILED-forever case as an unassigned port. Doesn't matter in
       practice today: every board with HELM_HAS_GPS_UART_TRANSPORT set
       also has HELM_FEATURE_PARAMS_PERSIST on. */

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(GPS_TASK_PERIOD_MS));

        if (gps_uart_bound) {
            while (board_gps_uart_available()) {
                gps_process_byte(board_gps_uart_read_byte());
            }
        }

        xQueueOverwrite(gps_queue, &latestFix);
        supervisor_kick(handle);
    }
}

void gps_start(void) {
    gps_queue = xQueueCreate(1, sizeof(GpsFix));

    latestFix.status = SENSOR_STATUS_FAILED;
    xQueueOverwrite(gps_queue, &latestFix);

    /* configMINIMAL_STACK_SIZE * 4, not the bare minimum -- this task's
       NMEA parsing chain (gps_process_sentence -> parse_gga/parse_rmc ->
       strtod) is real string/float parsing, same order of stack demand
       as cli.c's own shell task (its own xTaskCreate call uses the same
       *4 multiplier for the same reason: strtoul() and friends need more
       than the heartbeat task's bare-minimum stack). */
    xTaskCreate(gps_task, "gps", configMINIMAL_STACK_SIZE * 4, NULL, GPS_TASK_PRIORITY, NULL);
}

void gps_get_latest(GpsFix *out) {
    xQueuePeek(gps_queue, out, 0);
}

void gps_get_counters(uint32_t *bytesReceived, uint32_t *validSentences) {
    *bytesReceived = bytesReceivedCount;
    *validSentences = validSentenceCount;
}

#endif /* HELM_HAS_GPS */
