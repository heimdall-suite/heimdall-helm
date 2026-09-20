#ifndef HELM_MAPPING_H
#define HELM_MAPPING_H

#include <stdint.h>
#include "rx.h"

/* Function/input mapping stage (issue #34) -- assigns semantic meaning
   to raw RX channels via a table, rather than treating every channel as
   passthrough (issue #7's original stub). See .docs/architecture/
   receiver-to-servo.md's "Function/input mapping" section.

   First-pass scope: one real mode function, one placeholder target -- a
   hardcoded Pitch mode on CH9 and a Pitch target on CH4, both throwaway
   channel-index choices (mapping.c's own comment), not a real per-user
   assignment -- everything else stays passthrough. `channels[]` below is
   still every raw channel unchanged (including CH9/CH4's own raw
   values), same as the old stub -- #36 (output mapping) decides what, if
   anything, still reads those two channels as passthrough too; this
   stage doesn't hide them. Not yet a generic, configurable table
   (params-persist-backed, per lib/params's own roadmap) -- that's this
   issue's own follow-up once the mechanism proves out.

   Failsafe substitution for pitchMode/pitchTarget happens here (mode
   forces to Off, target forces to a safe centered setpoint) --
   deliberately NOT true for passthrough channels anymore, which get
   their own per-physical-slot hold-vs-fixed-preset substitution at the
   output mapping stage (#36) instead. See receiver-to-servo.md's
   "Failsafe" section for why the split, not one stage owning all of it.

   Reuses RxStatus -- the same two-tier OK/FAILSAFE concept
   module-architecture.md generalizes to every module, not something
   RX-specific. */
typedef enum {
    PITCH_MODE_OFF,
    PITCH_MODE_LIMIT,
    PITCH_MODE_ACTIVE,
} PitchMode;

typedef struct {
    uint16_t channels[RX_MAX_CHANNELS]; /* raw passthrough, every channel */
    PitchMode pitchMode;                /* from CH9, forced OFF on failsafe */
    uint16_t pitchTarget;               /* from CH4, forced to a safe centered
                                            setpoint on failsafe */
    RxStatus status;
} MappingFrame;

void mapping_start(void);
void mapping_get_latest(MappingFrame *out);

#endif /* HELM_MAPPING_H */
