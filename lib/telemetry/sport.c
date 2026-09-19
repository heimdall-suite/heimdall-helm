#include "sport.h"

#include "board_features.h"

/* Whole-file guard -- see sport.h's own comment for why. Compiles to an
   empty translation unit on any board without a ported
   board_sport_uart_* transport (board.h). */
#if HELM_HAS_SPORT_UART

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
                TelemetryEntry entry;
                telemetry_get(TELEM_FIELD_TEST, &entry);

                uint8_t frame[SPORT_MAX_STUFFED_BYTES];
                uint8_t const length = sport_build_data_frame(frame, SPORT_TEST_DATA_ID, (int32_t)entry.value);
                board_sport_uart_write(frame, length);
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
