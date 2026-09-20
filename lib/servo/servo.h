#ifndef HELM_SERVO_H
#define HELM_SERVO_H

#include <stdint.h>
#include "board_features.h"
#include "rx.h"

/* Real PWM servo driver (issue #31), one shared implementation for both
   boards -- standard STM32 timer output-compare PWM (50Hz/250Hz/333Hz
   frame, microsecond pulse width via ARR/CCR) is identical HAL/LL
   peripheral behavior on F1 and H7 alike; only which physical pin/timer/
   channel backs each slot differs per board (servo.c's own per-board
   padConfigs[] table). See .docs/architecture/receiver-to-servo.md's
   "Servo driver" section: it doesn't know or care whether a value came
   from passthrough or a control loop, so there's nothing protocol-
   specific here even now that it's real.

   PWM frame rate is one persisted setting for every slot on every timer,
   not a per-slot or per-timer one -- period is a per-timer property
   (servo.c's own comment), shared across every channel on it, so slots
   sharing a timer group can never run at different rates regardless; a
   single param is the only shape that's ever actually valid here.
   SERVO_RATE_50HZ (the safe default -- works with any servo, including
   plain analog ones), SERVO_RATE_250HZ, or SERVO_RATE_333HZ -- different
   digital servos support different faster frames for lower latency (this
   project's own bench mix: AGFRC B24CLM/B44DLM at 333Hz, a BMS-760MG at
   250Hz), but only if EVERY servo sharing a given timer group actually
   supports the selected rate; that's a fact about your specific servo
   loadout this driver has no way to verify, not something bench-
   confirmable in general -- put servos with different rate ceilings on
   different timer groups if you need to run them at different rates.

   Values are 0/1/3, not 0/1/2 -- value 2 is deliberately left unassigned
   (the user's own numbering choice when this was scoped), not a gap to
   fill in without asking first.

   Read once at servo_start() from lib/params' PARAM_SERVO_RATE when
   HELM_FEATURE_PARAMS_PERSIST is on, else always 50Hz -- same "persisted
   override, compile-time-safe fallback, read once at container-start"
   shape issue #10 already established for PARAM_INPUT_MODE. Takes effect
   on next boot only, same reason: servo_start() runs once before the
   scheduler starts anything that could change it later. */
#define SERVO_RATE_50HZ 0U
#define SERVO_RATE_250HZ 1U
#define SERVO_RATE_333HZ 3U

/* servos[] values are real microseconds (1 tick == 1us, by construction
   of servo.c's own per-timer prescaler), ready to write into a timer's
   CCR register directly -- output.h's OutputFrame carries the same
   convention now that this issue exists to receive it (previously a
   placeholder raw-tick-like convention, per that file's own now-stale
   comment).

   Sized to HELM_SERVO_COUNT (board_features.h), not RX_MAX_CHANNELS --
   one entry per physical servo, matching output.h's OutputFrame (issue
   #36) exactly, since servo.c's own memcpy() assumes the two match
   byte-for-byte. */
typedef struct {
    uint16_t servos[HELM_SERVO_COUNT];
    RxStatus status;
} ServoFrame;

void servo_start(void);
void servo_get_latest(ServoFrame *out);

#endif /* HELM_SERVO_H */
