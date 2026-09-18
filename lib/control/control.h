#ifndef HELM_CONTROL_H
#define HELM_CONTROL_H

#include <stdint.h>
#include "rx.h"

/* Control loop stand-in (issue #7) -- passthrough only, no PID/control
   math yet (see .docs/architecture/control-loops.md). Real control loops
   only ever see channels mapped to a mode/target function; #7's mapping
   stub never assigns one (everything is passthrough), so this stage has
   nothing real to compute yet. It still sits in the chain, wired through
   the full module/supervisor/queue plumbing, purely to prove that
   plumbing reaches every stage before real control-loop math (a later
   issue) replaces this body. */
typedef struct {
    uint16_t channels[RX_MAX_CHANNELS];
    RxStatus status;
} ControlFrame;

void control_start(void);
void control_get_latest(ControlFrame *out);

#endif /* HELM_CONTROL_H */
