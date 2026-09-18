#ifndef HELM_OUTPUT_H
#define HELM_OUTPUT_H

#include <stdint.h>
#include "rx.h"

/* Output mapping stage stub (issue #7) -- passthrough only, no real
   per-servo table yet: one array slot per input channel, copied straight
   through from the control stage. See .docs/architecture/
   receiver-to-servo.md's "Output mapping" section for the real job this
   will eventually do (control-loop output + passthrough channels mapped
   onto physical servo slots, generally fewer than the number of input
   channels). */
typedef struct {
    uint16_t servos[RX_MAX_CHANNELS];
    RxStatus status;
} OutputFrame;

void output_start(void);
void output_get_latest(OutputFrame *out);

#endif /* HELM_OUTPUT_H */
