#ifndef HELM_OUTPUT_H
#define HELM_OUTPUT_H

#include <stdint.h>
#include "board_features.h"
#include "rx.h"

/* Output mapping stage (issue #36) -- maps named signals (mapping.c's
   passthrough channels, or control.c's control-loop outputs) onto
   physical servo slots, per .docs/architecture/receiver-to-servo.md's
   "Output mapping" section. Real per-slot table lives in output.c
   (slotConfigs[], not exposed here) -- the servo driver, and anything
   else reading OutputFrame, only ever sees the final values, same
   "doesn't know or care where a value came from" discipline
   receiver-to-servo.md's "Servo driver" section already describes.

   Also owns, per slot (output.c's own comment for the full reasoning):
   - failsafe substitution: hold-last vs. a fixed configured preset,
     for a slot whose source (a passthrough channel, or a control loop)
     currently has no valid value -- moved here from mapping.c (#34) since
     this is the stage that actually knows which physical slot(s) a
     signal lands on.
   - direction/reverse: some physical servos are mounted such that a
     normal command drives them backwards from what's intended.

   HELM_SERVO_COUNT (board_features.h) sizes servos[] -- a real per-board
   hardware fact (how many physical servo connectors this board has),
   generally fewer than RX_MAX_CHANNELS, unlike every earlier stage in
   this chain which stayed sized to the RX channel count. servo.h's
   ServoFrame is sized the same way, for the same reason. */
typedef struct {
    uint16_t servos[HELM_SERVO_COUNT];
    RxStatus status;
} OutputFrame;

void output_start(void);
void output_get_latest(OutputFrame *out);

#endif /* HELM_OUTPUT_H */
