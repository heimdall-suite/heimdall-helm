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

/* Real per-servo physical units (issue #31): standard hobby PWM servo
   range, microseconds, ready to write into servo.c's own timer CCR
   registers with no further conversion -- servo.c's own SERVO_PWM_
   SAFE_CENTER_US matches OUTPUT_SERVO_CENTER_US here deliberately.
   Replaces the placeholder raw-tick-like convention every output range
   used before this issue (output.h's own comment already flagged this
   as deferred to #31). */
#define OUTPUT_SERVO_MIN_US 1000U
#define OUTPUT_SERVO_CENTER_US 1500U
#define OUTPUT_SERVO_MAX_US 2000U

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
   this). */
typedef struct {
    const char *name;
    OutputSourceType source;
    uint8_t channelIndex; /* only meaningful when source == OUTPUT_SOURCE_PASSTHROUGH */

    /* Issue #37 -- optional, only meaningful/used when source is a
       function (not OUTPUT_SOURCE_PASSTHROUGH, whose source range is
       always RX_CHANNEL_RAW_MIN/CENTER/MAX -- an override here would be
       meaningless and is ignored). Lets a future function with a
       genuinely different native output range declare it, instead of
       being silently misread as RX ticks. Unset for every slot in this
       pass -- control.c's Pitch loop still genuinely emits
       RX_CHANNEL_RAW-range values (its own placeholder body just passes
       pitchTarget through), so it correctly falls back to that same
       default rather than needing an override yet. */
    bool hasSourceRangeOverride;
    uint16_t sourceMin;
    uint16_t sourceCenter;
    uint16_t sourceMax;

    /* This slot's own physical endpoint/subtrim calibration (issue #37)
       -- always used, regardless of source. scale_slot() below maps
       sourceMin/Center/Max onto these three points as two independent
       linear segments (real RC endpoint+subtrim convention: trimming
       the center doesn't require min/max to stay symmetric around it),
       not one straight line across the whole span. Placeholder units --
       same raw-tick-like convention every other stage uses until #31
       decides real per-servo physical units. */
    uint16_t outputMin;
    uint16_t outputCenter;
    uint16_t outputMax;

    bool reversed;

    OutputFailsafePolicy failsafePolicy;
    /* Only meaningful when failsafePolicy == OUTPUT_FAILSAFE_FIXED -- a
       literal physical position in THIS slot's own output range (issue
       #37), applied with no scaling or reverse at all. FIXED means "snap
       to this known-safe physical spot," not "as if the source read this
       value." */
    uint16_t failsafeFixedValue;
} OutputSlotConfig;

/* Issue #36/#37/#31's first-pass hardcoded table -- same "prove the
   mechanism, real per-user config is later params-backed work" scope
   #34/#35 used for their own tables. One control-loop-fed slot (Pitch,
   #35); every other slot is a straight passthrough of the same-numbered
   channel. Every slot's output range is the standard hobby-servo
   1000/1500/2000us endpoint+center (issue #31 -- real physical units
   now that a real PWM driver exists to receive them, not the placeholder
   raw-tick-like convention #36/#37 used before this). S1/OUT1 is
   deliberately configured reversed, fixed-failsafe, AND with a modest
   asymmetric subtrim (1000/1550/2000, not a plain symmetric 1500
   center) -- safe, plausible real servo-trim numbers, not the wild
   demo range #37 used before real PWM existed to actually receive
   them, chosen so this pass still exercises piecewise scaling, not just
   an identity passthrough that happens to look right by coincidence.

   Per-board table, not one shared array -- HELM_SERVO_COUNT differs
   (board_features.h), and output.c is a single shared file (no
   lib_ignore/custom_helm_output chip-style split, since the difference
   between boards here is just a data table, not different logic) --
   same `#if defined(STM32H7)` chip-family branching src/main.c already
   uses for its own per-family HAL include. */
/* Slot names are the board's REAL physical silkscreen labels (S3-S10),
   NOT a generic S1-S8 index -- this board has no S1/S2 servo pads at
   all (see servo.c's own padConfigs[] comment: "Output3 (PA0/TIM5_CH1)
   is bench-confirmed..."). An earlier pass of this table used S1-S8,
   which matched nothing on the physical board and cost real bench time
   chasing a "no PWM signal" that was actually a probe on a nonexistent
   pad -- exactly the "wrong pairing hidden behind a table nothing
   double-checks against the datasheet" failure mode this project's own
   servo.c comments already warn about, just one level up from where
   that warning was originally aimed. */
#if defined(STM32H7)
static const OutputSlotConfig slotConfigs[HELM_SERVO_COUNT] = {
    {.name = "S3",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 0,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = 1550,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .reversed = true,
     .failsafePolicy = OUTPUT_FAILSAFE_FIXED,
     .failsafeFixedValue = 1550},
    {.name = "S4",
     .source = OUTPUT_SOURCE_PITCH_CONTROL,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "S5",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 2,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "S6",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 3,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "S7",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 4,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "S8",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 5,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "S9",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 6,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "S10",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 7,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
};
#elif defined(STM32F1)
static const OutputSlotConfig slotConfigs[HELM_SERVO_COUNT] = {
    {.name = "OUT1",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 0,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = 1550,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .reversed = true,
     .failsafePolicy = OUTPUT_FAILSAFE_FIXED,
     .failsafeFixedValue = 1550},
    {.name = "OUT2",
     .source = OUTPUT_SOURCE_PITCH_CONTROL,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "OUT3",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 2,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "OUT4",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 3,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "OUT5",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 4,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
    {.name = "OUT6",
     .source = OUTPUT_SOURCE_PASSTHROUGH,
     .channelIndex = 5,
     .outputMin = OUTPUT_SERVO_MIN_US,
     .outputCenter = OUTPUT_SERVO_CENTER_US,
     .outputMax = OUTPUT_SERVO_MAX_US,
     .failsafePolicy = OUTPUT_FAILSAFE_HOLD},
};
#else
#error "output.c needs a slotConfigs[] table for this chip family -- see matek_h743/afroflight32's own for the shape"
#endif

static QueueHandle_t output_queue;

/* Per-slot memory for OUTPUT_FAILSAFE_HOLD (issue #37) -- the final,
   post-scale-and-reverse physical value this slot last actually wrote
   out, NOT the pre-scale raw source value (#36's own original
   implementation) -- "hold" means "repeat the last real physical
   position," which only the post-scale value actually represents once
   slots have their own real endpoints. Seeded to each slot's own
   outputCenter at start (output_start()), same "safe centered default
   until a real value exists" reasoning mapping.c's own failsafe setpoint
   uses. */
static uint16_t lastGoodValue[HELM_SERVO_COUNT];

/* Piecewise linear scale (issue #37): sourceMin->outputMin,
   sourceCenter->outputCenter, sourceMax->outputMax, as two independent
   segments -- real RC endpoint+subtrim convention, not one straight line
   across the whole span, so trimming the center doesn't require min/max
   to stay symmetric around it. Integer math throughout (this project's
   established style -- see e.g. diag_baro()/diag_telemetry()'s own
   "avoid %f" comments -- and afroflight32 has no hardware FPU to begin
   with). A degenerate half-span (sourceCenter == sourceMin or ==
   sourceMax, a misconfigured table) falls back to that segment's own
   output endpoint rather than dividing by zero. */
static uint16_t scale_slot(int32_t raw, int32_t srcMin, int32_t srcCenter, int32_t srcMax, int32_t dstMin,
                            int32_t dstCenter, int32_t dstMax) {
    if (raw <= srcCenter) {
        int32_t const srcSpan = srcCenter - srcMin;
        if (srcSpan == 0) {
            return (uint16_t)dstMin;
        }
        return (uint16_t)(dstMin + ((raw - srcMin) * (dstCenter - dstMin)) / srcSpan);
    }

    int32_t const srcSpan = srcMax - srcCenter;
    if (srcSpan == 0) {
        return (uint16_t)dstMax;
    }
    return (uint16_t)(dstCenter + ((raw - srcCenter) * (dstMax - dstCenter)) / srcSpan);
}

/* Reverse mirrors the slot's OWN output range (issue #37) -- NOT the
   global RX_CHANNEL_RAW_MIN/MAX (#36's own original implementation),
   which stopped being correct the moment slots got their own real
   endpoints: by the time reverse runs, a value is already expressed in
   this slot's physical range, not the RX raw range. */
static uint16_t apply_reverse(uint16_t value, uint16_t outputMin, uint16_t outputMax) {
    return (uint16_t)(outputMin + outputMax - value);
}

/* Resolves one slot's final output value: substitutes per its own
   failsafe policy if its source has nothing valid right now, else scales
   the raw source value through this slot's own endpoint/subtrim
   calibration; then applies reverse if configured -- except for a FIXED
   failsafe value, which is already a literal physical position (issue
   #37) and skips scaling/reverse entirely. Also updates lastGoodValue[
   slot] with the final value actually produced, whenever the source IS
   valid, so a later failsafe drop holds the real last physical position,
   not a stale substituted one. */
static uint16_t resolve_slot(uint8_t slot, bool sourceValid, uint16_t rawValue) {
    const OutputSlotConfig *cfg = &slotConfigs[slot];

    if (!sourceValid) {
        if (cfg->failsafePolicy == OUTPUT_FAILSAFE_FIXED) {
            return cfg->failsafeFixedValue;
        }
        return lastGoodValue[slot];
    }

    uint16_t srcMin = RX_CHANNEL_RAW_MIN;
    uint16_t srcCenter = RX_CHANNEL_RAW_CENTER;
    uint16_t srcMax = RX_CHANNEL_RAW_MAX;
    if (cfg->source != OUTPUT_SOURCE_PASSTHROUGH && cfg->hasSourceRangeOverride) {
        srcMin = cfg->sourceMin;
        srcCenter = cfg->sourceCenter;
        srcMax = cfg->sourceMax;
    }

    uint16_t scaled = scale_slot(rawValue, srcMin, srcCenter, srcMax, cfg->outputMin, cfg->outputCenter, cfg->outputMax);
    uint16_t const finalValue = cfg->reversed ? apply_reverse(scaled, cfg->outputMin, cfg->outputMax) : scaled;

    lastGoodValue[slot] = finalValue;
    return finalValue;
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
        lastGoodValue[i] = slotConfigs[i].outputCenter;
        initial.servos[i] = resolve_slot(i, false, 0);
    }
    xQueueOverwrite(output_queue, &initial);

    xTaskCreate(output_task, "output", configMINIMAL_STACK_SIZE, NULL,
                OUTPUT_TASK_PRIORITY, NULL);
}

void output_get_latest(OutputFrame *out) {
    xQueuePeek(output_queue, out, 0);
}
