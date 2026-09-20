#include "sport.h"

#include "board_features.h"

/* Whole-file guard -- see sport.h's own comment for why. Compiles to an
   empty translation unit on any board without a ported
   board_sport_uart_* transport (board.h). */
#if HELM_HAS_SPORT_UART

#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "telemetry.h"

/* Physical ID this board answers under -- 0x1B, one of four values
   Betaflight reserves for FC-as-sensor use (FSSP_SENSOR_ID1),
   bench-confirmed working against a real receiver on
   aoa-boat-controller's identical UART7/PE8 wiring (four candidate IDs
   tried there; all behaved identically -- see that project's
   docs/decisions.md). Not yet independently re-chosen for this
   project's own bus -- same proven-safe-placeholder status it carried
   there. */
#define SPORT_PHYSICAL_ID 0x1B

/* Plain-DIY data ID (0x5100-0x52FF range) for TELEM_FIELD_TEST -- same
   ID and justification aoa-boat-controller used for its own heartbeat
   sensor: no native FrSky type applies, purpose is purely an
   unambiguous "is this alive" signal on the radio's telemetry screen.
   Deliberately NOT in EdgeTX's DIY_STREAM range (0x5000-0x50FF) -- that
   project's docs/decisions.md documents DIY_STREAM funneling into a
   shared, easily-starved Lua queue. Irrelevant here today (no push/MSP
   path exists at all, see sport.h), but worth staying out of from day
   one rather than relearning that the hard way once one does. */
#define SPORT_TEST_DATA_ID 0x5100

/* Issue #33's first two real fields.

   TELEM_FIELD_BARO_TEMPERATURE uses FrSky's own native T1 data ID
   (0x0400) rather than a DIY one -- real type exists, value is plain
   whole degrees C with no scaling, so a stock radio labels and displays
   it correctly with no Lua script needed, unlike the DIY ones.

   TELEM_FIELD_BARO_PRESSURE has no matching native type: FrSky's
   altitude/vario sensors report a derived altitude, not raw station
   pressure, and deriving true altitude needs a reference-pressure/
   calibration design that doesn't exist yet (explicitly out of scope
   for this issue) -- so this stays a DIY ID, next one up from
   SPORT_TEST_DATA_ID in the same 0x51xx block, whole Pascals (plenty of
   resolution for "is this sane," no consumer/Lua script parses it yet,
   same as TEST's own value). */
#define SPORT_BARO_TEMPERATURE_DATA_ID 0x0400
#define SPORT_BARO_PRESSURE_DATA_ID 0x5101

/* Issue #41 -- native FrSky data IDs for battery/GPS, verified against
   Betaflight's own telemetry/smartport.c (src/main/telemetry/smartport.c,
   master branch, checked 2026-09-21) rather than guessed -- this project's
   own established standard for any protocol fact (same source #18/#33's
   frame-building/checksum code was itself ported from). Using real native
   IDs, not DIY ones, means these show up correctly labeled/scaled on a
   stock radio with no Lua script needed, same reasoning
   SPORT_BARO_TEMPERATURE_DATA_ID's own T1 choice already used. */
#define SPORT_VFAS_DATA_ID 0x0210     /* FSSP_DATAID_VFAS -- pack voltage, 0.01V units */
#define SPORT_CURRENT_DATA_ID 0x0200  /* FSSP_DATAID_CURRENT -- pack current, 0.1A units */
#define SPORT_LATLONG_DATA_ID 0x0800  /* FSSP_DATAID_LATLONG -- same ID sent twice per full
                                          fix, once for latitude and once for longitude, see
                                          sport_encode_gps_coord()'s own comment */
#define SPORT_GPS_ALT_DATA_ID 0x0820  /* FSSP_DATAID_GPS_ALT -- centimeters */
#define SPORT_SPEED_DATA_ID 0x0830    /* FSSP_DATAID_SPEED -- knots * 1000 */

/* How to turn a telemetry.h field's raw physical-unit value (volts,
   amps, degrees, meters, m/s -- see each TELEM_FIELD_* comment) into
   the integer this data ID actually expects on the wire. Most existing
   fields (test, baro) already store the exact value S.Port wants and
   need no conversion; the new ones in this issue don't. */
typedef enum {
    SPORT_ENCODE_RAW,               /* cast straight to int32 -- test/baro's existing shape */
    SPORT_ENCODE_CENTIVOLTS,        /* volts -> volts*100 */
    SPORT_ENCODE_DECIAMPS,          /* amps -> amps*10 */
    SPORT_ENCODE_ALT_CM,            /* meters -> centimeters */
    SPORT_ENCODE_SPEED_KNOTS_X1000, /* m/s -> knots*1000 */
    SPORT_ENCODE_GPS_LAT,           /* degrees -> FrSky packed lat/lon, bit31 clear */
    SPORT_ENCODE_GPS_LON,           /* degrees -> FrSky packed lat/lon, bit31 set */
} SportEncodeKind;

/* One entry per field this board's single physical ID (SPORT_PHYSICAL_ID)
   can report -- real multi-value FrSky sensors (e.g. an FLVSS cycling
   through cell voltages) answer one data ID per poll and rotate, rather
   than trying to cram everything into one poll's response; sport_task()
   below does the same. TELEM_FIELD_GPS_LATITUDE/_LONGITUDE deliberately
   share SPORT_LATLONG_DATA_ID -- that's the real FrSky protocol shape
   (one ID, alternating meaning), not a mistake; sport_encode_value()
   below is what tells them apart. TELEM_FIELD_GPS_SATELLITES has no
   entry here -- no native S.Port slot exists for it (telemetry.h's own
   comment), CLI diag only. */
static const struct {
    TelemetryField field;
    uint16_t dataId;
    SportEncodeKind encode;
} sport_fields[] = {
    {TELEM_FIELD_TEST, SPORT_TEST_DATA_ID, SPORT_ENCODE_RAW},
    {TELEM_FIELD_BARO_PRESSURE, SPORT_BARO_PRESSURE_DATA_ID, SPORT_ENCODE_RAW},
    {TELEM_FIELD_BARO_TEMPERATURE, SPORT_BARO_TEMPERATURE_DATA_ID, SPORT_ENCODE_RAW},
#if HELM_HAS_BATTERY_SENSE
    {TELEM_FIELD_BATTERY_VOLTAGE, SPORT_VFAS_DATA_ID, SPORT_ENCODE_CENTIVOLTS},
    {TELEM_FIELD_BATTERY_CURRENT, SPORT_CURRENT_DATA_ID, SPORT_ENCODE_DECIAMPS},
#endif
#if HELM_HAS_GPS
    {TELEM_FIELD_GPS_LATITUDE, SPORT_LATLONG_DATA_ID, SPORT_ENCODE_GPS_LAT},
    {TELEM_FIELD_GPS_LONGITUDE, SPORT_LATLONG_DATA_ID, SPORT_ENCODE_GPS_LON},
    {TELEM_FIELD_GPS_ALTITUDE, SPORT_GPS_ALT_DATA_ID, SPORT_ENCODE_ALT_CM},
    {TELEM_FIELD_GPS_SPEED, SPORT_SPEED_DATA_ID, SPORT_ENCODE_SPEED_KNOTS_X1000},
#endif
};
#define SPORT_FIELD_COUNT (sizeof(sport_fields) / sizeof(sport_fields[0]))

/* Packs a signed decimal-degrees value into FrSky's real LATLONG wire
   format -- ported verbatim (not re-derived) from Betaflight's
   telemetry/smartport.c FSSP_DATAID_LATLONG case: magnitude in degrees
   is rescaled to minutes*10000 via the exact same "(x + x/2) / 25"
   fixed-point trick that file uses (equivalent to x*0.06, i.e.
   degrees*1e7 -> minutes*10000, but division-by-power-of-2-friendly),
   bit30 set if the original value was negative (S or W), bit31 set only
   for longitude (0 for latitude) -- that source's own comment: "the MSB
   of the sent uint32_t helps FrSky keep track". isLongitude selects
   which of those last two bits this call sets; the caller (sport_encode_
   value()) is responsible for actually alternating between the two
   across polls, same "same ID sent twice" shape that source uses. */
static uint32_t sport_encode_gps_coord(float degrees, bool isLongitude) {
    float const absDegrees = degrees < 0.0f ? -degrees : degrees;
    uint32_t const scaledE7 = (uint32_t)(absDegrees * 10000000.0f);
    uint32_t packed = (scaledE7 + scaledE7 / 2) / 25U;

    if (isLongitude) {
        packed |= 0x80000000U;
    }
    if (degrees < 0.0f) {
        packed |= 0x40000000U;
    }
    return packed;
}

/* Converts one telemetry.h entry's raw physical-unit value into the
   int32_t sport_build_data_frame() should actually send, per this
   field's SportEncodeKind. Scale factors (100, 10, 100, 1944.0/100)
   match Betaflight's own smartport.c comments verbatim (see each
   SportEncodeKind enumerator's own comment above) -- not independently
   derived. */
static int32_t sport_encode_value(SportEncodeKind encode, float value) {
    switch (encode) {
    case SPORT_ENCODE_CENTIVOLTS:
        return (int32_t)(value * 100.0f);
    case SPORT_ENCODE_DECIAMPS:
        return (int32_t)(value * 10.0f);
    case SPORT_ENCODE_ALT_CM:
        return (int32_t)(value * 100.0f);
    case SPORT_ENCODE_SPEED_KNOTS_X1000:
        /* m/s -> knots*1000: 1 m/s = 1.943844 knots, same conversion
           constant smartport.c's own cm/s-based comment uses, rescaled
           here since gps.c's own TELEM_FIELD_GPS_SPEED is m/s, not
           cm/s. */
        return (int32_t)(value * 1943.844f);
    case SPORT_ENCODE_GPS_LAT:
        return (int32_t)sport_encode_gps_coord(value, false);
    case SPORT_ENCODE_GPS_LON:
        return (int32_t)sport_encode_gps_coord(value, true);
    case SPORT_ENCODE_RAW:
    default:
        return (int32_t)value;
    }
}

#define SPORT_START_STOP 0x7E
#define SPORT_DLE 0x7D
#define SPORT_DLE_XOR 0x20
#define SPORT_DATA_FRAME 0x10
#define SPORT_MAX_STUFFED_BYTES 16 /* worst case: 8 raw bytes, every one stuffed to 2 */

/* Placeholder priority -- same as every other stub-chain task (RX,
   mapping, ...), all still priority 1: module-architecture.md's
   priority tiers aren't numerically decided yet, see that doc's "Open /
   not yet decided" list. Real spec margin is generous regardless: this
   task only has to wake and respond somewhere inside S.Port's ~6.5ms
   per-poll slack (ArduPilot's AP_Frsky_SPort.cpp, cross-checked on
   aoa-boat-controller's own bench at 5us reaction latency for a
   materially similar dispatch path) -- a FreeRTOS task-notification
   wake at priority 1, with nothing else on this board doing multi-
   millisecond-long work, is comfortably inside that. */
#define SPORT_TASK_PRIORITY 1

/* Appends `byte` to `bytes`, stuffed if it collides with a reserved
   value, and folds the raw (unstuffed) value into `checksum` -- matches
   Betaflight's own checksum ordering (sums before stuffing is applied).
   Ported from aoa-boat-controller's lib/SPort/Shared/SportFrame
   (its own header cites Betaflight's telemetry/smartport.c and
   rx/frsky_crc.c as source, cross-checked there against ArduPilot's
   AP_Frsky_SPort.cpp too -- not derived from scratch here). */
static void sport_append_stuffed(uint8_t *bytes, uint8_t *length, uint8_t byte, uint16_t *checksum) {
    *checksum = (uint16_t)(*checksum + byte);
    if (byte == SPORT_DLE || byte == SPORT_START_STOP) {
        bytes[(*length)++] = SPORT_DLE;
        bytes[(*length)++] = (uint8_t)(byte ^ SPORT_DLE_XOR);
    } else {
        bytes[(*length)++] = byte;
    }
}

/* Builds a data frame (frame type 0x10) for dataId/value -- checksum
   computed and byte-stuffing applied, ready to write to the transport
   byte-for-byte in order. Classic FrSky additive checksum: sum bytes as
   uint16_t, fold to 8 bits, 0xFF - result. Returns the stuffed length. */
static uint8_t sport_build_data_frame(uint8_t *bytes, uint16_t dataId, int32_t value) {
    uint8_t length = 0;
    uint16_t checksum = 0;

    sport_append_stuffed(bytes, &length, SPORT_DATA_FRAME, &checksum);
    sport_append_stuffed(bytes, &length, (uint8_t)(dataId & 0xFF), &checksum);
    sport_append_stuffed(bytes, &length, (uint8_t)((dataId >> 8) & 0xFF), &checksum);
    sport_append_stuffed(bytes, &length, (uint8_t)(value & 0xFF), &checksum);
    sport_append_stuffed(bytes, &length, (uint8_t)((value >> 8) & 0xFF), &checksum);
    sport_append_stuffed(bytes, &length, (uint8_t)((value >> 16) & 0xFF), &checksum);
    sport_append_stuffed(bytes, &length, (uint8_t)((value >> 24) & 0xFF), &checksum);

    while (checksum > 0xFF) {
        checksum = (uint16_t)((checksum & 0xFF) + (checksum >> 8));
    }
    uint8_t const checksumByte = (uint8_t)(0xFF - checksum);

    /* The checksum byte isn't itself folded into the running checksum --
       it IS the checksum -- so pass a throwaway accumulator, just
       reusing sport_append_stuffed() for its stuffing logic. */
    uint16_t discard = 0;
    sport_append_stuffed(bytes, &length, checksumByte, &discard);

    return length;
}

/* Poll-detection state machine, one byte at a time -- mirrors
   aoa-boat-controller's own SportSensor::checkPoll() (poll-only half;
   this project doesn't implement the write-direction/push-frame path,
   see sport.h). The poll marker and physical-ID byte are never
   themselves stuffed (specifically chosen values that never collide
   with SPORT_START_STOP) -- stuffing only applies to frame payload
   bytes, so this check doesn't need any destuffing logic of its own. */
typedef enum {
    SPORT_RX_IDLE,
    SPORT_RX_AWAITING_ID,
} SportRxState;

static SportRxState sportRxState = SPORT_RX_IDLE;

/* Bench diagnostics -- see sport.h's own comment. */
static uint32_t sportPollMarkerCount;
static uint32_t sportPollMatchCount;

/* Returns true exactly once per poll addressed to us. */
static bool sport_check_poll(uint8_t b) {
    if (b == SPORT_START_STOP) {
        sportRxState = SPORT_RX_AWAITING_ID;
        sportPollMarkerCount++;
        return false;
    }

    if (sportRxState == SPORT_RX_AWAITING_ID) {
        sportRxState = SPORT_RX_IDLE;
        bool const matched = b == SPORT_PHYSICAL_ID;
        if (matched) {
            sportPollMatchCount++;
        }
        return matched;
    }

    return false;
}

void sport_get_counters(uint32_t *pollMarkers, uint32_t *pollMatches) {
    *pollMarkers = sportPollMarkerCount;
    *pollMatches = sportPollMatchCount;
}

/* Round-robin index into sport_fields[] -- advances exactly once per poll
   addressed to us, regardless of whether that poll's chosen field ends
   up sending a frame (see below), so one persistently FAILED field can't
   starve the rotation by getting re-picked every time. */
static uint8_t sportFieldIndex;

static void sport_task(void *arg) {
    (void)arg;

    for (;;) {
        /* Blocks until board.c's UART7 ISR notifies us a byte arrived --
           see board_sport_uart_set_rx_task()'s own comment for why
           protocol logic lives here, in task context, and not in the
           ISR. */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (board_sport_uart_available()) {
            uint8_t const b = board_sport_uart_read_byte();
            if (sport_check_poll(b)) {
                uint8_t const fieldIndex = sportFieldIndex;
                sportFieldIndex = (uint8_t)((sportFieldIndex + 1) % SPORT_FIELD_COUNT);

                TelemetryEntry entry;
                telemetry_get(sport_fields[fieldIndex].field, &entry);

                /* Skip this poll's response entirely rather than send a
                   fabricated value -- a real device not answering every
                   single poll is normal S.Port behavior, unlike lying
                   about a field's value would be (baro.h's own comment
                   on this same discipline). */
                if (entry.status != TELEM_STATUS_FAILED) {
                    int32_t const wireValue = sport_encode_value(sport_fields[fieldIndex].encode, entry.value);
                    uint8_t frame[SPORT_MAX_STUFFED_BYTES];
                    uint8_t const length = sport_build_data_frame(frame, sport_fields[fieldIndex].dataId, wireValue);
                    board_sport_uart_write(frame, length);
                }
            }
        }
    }
}

void sport_start(void) {
    TaskHandle_t task;
    xTaskCreate(sport_task, "sport", configMINIMAL_STACK_SIZE, NULL, SPORT_TASK_PRIORITY, &task);

    /* Must be registered before board_sport_uart_init() enables RXNE --
       otherwise a poll byte could arrive before the ISR has a task to
       notify. */
    board_sport_uart_set_rx_task(task);
    board_sport_uart_init();
}

#endif /* HELM_HAS_SPORT_UART */
