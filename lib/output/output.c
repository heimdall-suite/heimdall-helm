#include "output.h"

#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "supervisor.h"
#include "mapping.h"
#include "control.h"

/* See mapping.c's own comment -- same placeholder-period-and-priority
   reasoning applies to every stage in this chain (issue #7). */
#define OUTPUT_TASK_PERIOD_MS 20
#define OUTPUT_TASK_PRIORITY 1

typedef enum {
    OUTPUT_SOURCE_PASSTHROUGH,
    OUTPUT_SOURCE_PITCH_CONTROL,
} OutputSourceType;

typedef enum {
    OUTPUT_FAILSAFE_HOLD,
    OUTPUT_FAILSAFE_FIXED,
} OutputFailsafePolicy;

/* Per-slot config -- private to this file (output.h's own comment: the
   servo driver, and everything upstream, doesn't need to know any of
   this). `channelIndex` only means anything when `source` is
   OUTPUT_SOURCE_PASSTHROUGH; `failsafeFixedValue` only when
   `failsafePolicy` is OUTPUT_FAILSAFE_FIXED. */
typedef struct {
    const char *name;
    OutputSourceType source;
    uint8_t channelIndex;
    bool reversed;
    OutputFailsafePolicy failsafePolicy;
    uint16_t failsafeFixedValue;
} OutputSlotConfig;

/* Issue #36's first-pass hardcoded table -- same "prove the mechanism,
   real per-user config is later params-backed work" scope #34/#35 used
   for their own tables. One control-loop-fed slot (Pitch, #35); every
   other slot is a straight passthrough of the same-numbered channel.
   Slot 1 is deliberately configured reversed + fixed-failsafe, not
   because it needs to be, but so this first pass actually exercises both
   new mechanisms on real hardware rather than leaving them unverified
   defaults nothing ever triggers.

   Per-board table, not one shared array -- HELM_SERVO_COUNT differs
   (board_features.h), and output.c is a single shared file (no
   lib_ignore/custom_helm_output chip-style split, since the difference
   between boards here is just a data table, not different logic) --
   same `#if defined(STM32H7)` chip-family branching src/main.c already
   uses for its own per-family HAL include. */
#if defined(STM32H7)
static const OutputSlotConfig slotConfigs[HELM_SERVO_COUNT] = {
    {"S1", OUTPUT_SOURCE_PASSTHROUGH, 0, true, OUTPUT_FAILSAFE_FIXED, RX_CHANNEL_RAW_CENTER},
    {"S2", OUTPUT_SOURCE_PITCH_CONTROL, 0, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"S3", OUTPUT_SOURCE_PASSTHROUGH, 2, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"S4", OUTPUT_SOURCE_PASSTHROUGH, 3, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"S5", OUTPUT_SOURCE_PASSTHROUGH, 4, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"S6", OUTPUT_SOURCE_PASSTHROUGH, 5, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"S7", OUTPUT_SOURCE_PASSTHROUGH, 6, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"S8", OUTPUT_SOURCE_PASSTHROUGH, 7, false, OUTPUT_FAILSAFE_HOLD, 0},
};
#elif defined(STM32F1)
static const OutputSlotConfig slotConfigs[HELM_SERVO_COUNT] = {
    {"OUT1", OUTPUT_SOURCE_PASSTHROUGH, 0, true, OUTPUT_FAILSAFE_FIXED, RX_CHANNEL_RAW_CENTER},
    {"OUT2", OUTPUT_SOURCE_PITCH_CONTROL, 0, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"OUT3", OUTPUT_SOURCE_PASSTHROUGH, 2, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"OUT4", OUTPUT_SOURCE_PASSTHROUGH, 3, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"OUT5", OUTPUT_SOURCE_PASSTHROUGH, 4, false, OUTPUT_FAILSAFE_HOLD, 0},
    {"OUT6", OUTPUT_SOURCE_PASSTHROUGH, 5, false, OUTPUT_FAILSAFE_HOLD, 0},
};
#else
#error "output.c needs a slotConfigs[] table for this chip family -- see matek_h743/afroflight32's own for the shape"
#endif

static QueueHandle_t output_queue;

/* Per-slot memory for OUTPUT_FAILSAFE_HOLD -- the raw (pre-reverse) value
   this slot's source last actually reported as valid. Seeded to
   RX_CHANNEL_RAW_CENTER at start (output_start()), same "safe centered
   default until a real value exists" reasoning mapping.c's own failsafe
   setpoint uses. */
static uint16_t lastGoodValue[HELM_SERVO_COUNT];

/* Reverse is a pure last-step transform, applied identically whether the
   value about to go out is a live passthrough/control value or a
   substituted hold/fixed one -- so a physically-reversed servo's
   failsafe position is still correct in real-world terms too. Mirrors
   the raw range symmetrically around its own midpoint; may need
   revisiting once #31 and real per-servo endpoint/trim config exist
   (output.h's own comment -- "center" itself isn't finally decided yet
   either). */
static uint16_t apply_reverse(uint16_t value) {
    return (uint16_t)(RX_CHANNEL_RAW_MIN + RX_CHANNEL_RAW_MAX - value);
}

/* Resolves one slot's final output value: substitutes per its own
   failsafe policy if its source has nothing valid right now, then
   applies reverse if configured. Also updates lastGoodValue[slot] when
   the source IS valid, so a later failsafe drop holds the real last
   value, not a stale substituted one. */
static uint16_t resolve_slot(uint8_t slot, bool sourceValid, uint16_t rawValue) {
    uint16_t finalValue;

    if (sourceValid) {
        lastGoodValue[slot] = rawValue;
        finalValue = rawValue;
    } else if (slotConfigs[slot].failsafePolicy == OUTPUT_FAILSAFE_FIXED) {
        finalValue = slotConfigs[slot].failsafeFixedValue;
    } else {
        finalValue = lastGoodValue[slot];
    }

    return slotConfigs[slot].reversed ? apply_reverse(finalValue) : finalValue;
}

static void output_task(void *arg) {
    (void)arg;

    OutputFrame fallback = {0};
    fallback.status = RX_STATUS_FAILSAFE;
    SupervisorHandle handle = supervisor_register("output", output_queue, &fallback,
                                                   sizeof(fallback),
                                                   pdMS_TO_TICKS(OUTPUT_TASK_PERIOD_MS * 3));

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(OUTPUT_TASK_PERIOD_MS));

        MappingFrame mapping;
        mapping_get_latest(&mapping);
        ControlFrame control;
        control_get_latest(&control);

        OutputFrame out;
        out.status = mapping.status;

        for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
            bool sourceValid;
            uint16_t rawValue;

            if (slotConfigs[i].source == OUTPUT_SOURCE_PITCH_CONTROL) {
                sourceValid = control.pitchActive;
                rawValue = control.pitchOutput;
            } else {
                sourceValid = (mapping.status == RX_STATUS_OK);
                rawValue = mapping.channels[slotConfigs[i].channelIndex];
            }

            out.servos[i] = resolve_slot(i, sourceValid, rawValue);
        }

        xQueueOverwrite(output_queue, &out);
        supervisor_kick(handle);
    }
}

void output_start(void) {
    output_queue = xQueueCreate(1, sizeof(OutputFrame));

    /* Seed the queue before anything downstream can peek it -- a
       length-1 overwrite queue only holds a valid value once something
       has actually written to it once. Every slot starts with no known-
       good value yet, so this seed goes through the exact same
       substitution path resolve_slot() uses at runtime (sourceValid =
       false), not a separately hand-rolled one. */
    OutputFrame initial = {0};
    initial.status = RX_STATUS_FAILSAFE;
    for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
        lastGoodValue[i] = RX_CHANNEL_RAW_CENTER;
        initial.servos[i] = resolve_slot(i, false, 0);
    }
    xQueueOverwrite(output_queue, &initial);

    xTaskCreate(output_task, "output", configMINIMAL_STACK_SIZE, NULL,
                OUTPUT_TASK_PRIORITY, NULL);
}

void output_get_latest(OutputFrame *out) {
    xQueuePeek(output_queue, out, 0);
}
