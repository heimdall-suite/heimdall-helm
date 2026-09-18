#ifndef HELM_MAPPING_H
#define HELM_MAPPING_H

#include <stdint.h>
#include "rx.h"

/* Function/input mapping stage stub (issue #7) -- no real mapping table
   yet (that's later, param-persist-adjacent work), so every channel is
   implicitly passthrough: this frame is structurally identical to
   RxFrame until a real table starts assigning some channels to named
   mode/target functions instead of raw passthrough. See .docs/
   architecture/receiver-to-servo.md's "Function/input mapping" section.
   Reuses RxStatus -- the same two-tier OK/FAILSAFE concept
   module-architecture.md generalizes to every module, not something
   RX-specific. */
typedef struct {
    uint16_t channels[RX_MAX_CHANNELS];
    RxStatus status;
} MappingFrame;

void mapping_start(void);
void mapping_get_latest(MappingFrame *out);

#endif /* HELM_MAPPING_H */
