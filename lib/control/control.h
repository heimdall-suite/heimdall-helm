#ifndef HELM_CONTROL_H
#define HELM_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "rx.h"

/* Control loop stage (issue #35) -- plumbs mapping.c's Pitch mode/target
   function (issue #34) through a per-loop output, but with a no-op
   placeholder body, not real PID/attitude math yet (see .docs/
   architecture/control-loops.md, still an open design). This issue's own
   scope is narrower than "real control loops": just prove a value flows
   from mapping's function output, through this stage's own per-loop
   state, ready for output mapping (#36) to consume -- same "prove the
   plumbing before the real logic exists" purpose issue #7 already served
   one level up.

   `pitchActive`/`pitchOutput` split, not a single value: control-loops.md
   already decided "a loop in Off mode produces no output," and mapping.c
   already forces PitchMode to Off during failsafe -- so this stage
   inherits failsafe-correctness for free, with no failsafe-awareness of
   its own, by just implementing that literally. `pitchActive == false`
   IS "no output"; `pitchOutput` is only meaningful when true. Placeholder
   body when active: a straight passthrough of mapping's pitchTarget, not
   a real PID compute.

   `channels[]` stays a full raw passthrough forward, same shape/values
   mapping.c's own MappingFrame carries -- #36 is expected to read
   passthrough channels via mapping_get_latest() directly instead (per
   receiver-to-servo.md's diagram: passthrough skips control loops
   entirely), so this field is likely to go away once #36 lands; kept for
   now purely so output.c/servo.c need zero changes until then, same
   "don't touch stages this issue doesn't own" discipline #34 used for
   MappingFrame. */
typedef struct {
    bool pitchActive;
    uint16_t pitchOutput;
    uint16_t channels[RX_MAX_CHANNELS];
    RxStatus status;
} ControlFrame;

void control_start(void);
void control_get_latest(ControlFrame *out);

#endif /* HELM_CONTROL_H */
