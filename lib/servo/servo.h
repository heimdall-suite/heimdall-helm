#ifndef HELM_SERVO_H
#define HELM_SERVO_H

#include <stdint.h>
#include "rx.h"

/* Servo driver stub (issue #7) -- records whatever the output mapping
   stage handed it instead of driving real PWM hardware (no real servo
   driver exists on any board yet). See .docs/architecture/
   receiver-to-servo.md's "Servo driver" section: it doesn't know or care
   whether a value came from passthrough or a control loop, so there's
   nothing protocol- or board-specific here even as a stub. */
typedef struct {
    uint16_t servos[RX_MAX_CHANNELS];
    RxStatus status;
} ServoFrame;

void servo_start(void);
void servo_get_latest(ServoFrame *out);

#endif /* HELM_SERVO_H */
