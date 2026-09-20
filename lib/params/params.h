#ifndef HELM_PARAMS_H
#define HELM_PARAMS_H

#include <stdbool.h>
#include <stdint.h>

/* Generic, board-agnostic persisted-parameter store (issue #32) -- get/
   set/enumerate over a compile-time registry, backed by whichever chip
   file (lib/params/stm32h7.c, stm32f1.c) this board's env selects via
   custom_helm_params_backend (see lib/README.md's "more than one chip in
   a subsystem" template; this file + params.c is the always-compiled
   shared half, same shape as lib/sensors/imu.h + imu.c). Gated
   end-to-end on board_features.h's HELM_FEATURE_PARAMS_PERSIST -- a
   board with the flag off never calls params_init(), never links this
   module in (see platformio.ini's lib_ignore), same "feature a board can
   decline" shape as HELM_FEATURE_CLI.

   Scope of this pass (issue #32): the generic store + one throwaway test
   param (PARAM_TEST_COUNTER), proven end-to-end via the CLI's new `param`
   command. Real consumers (input/function mapping, servo endpoints, PID
   gains) are deliberately NOT wired to this store yet -- separate,
   future issues build those records on top of this interface once it's
   proven. */

/* Every persisted value gets a stable id here -- the registry/CLI index
   into this, not into raw byte offsets. Append new params at the end,
   never renumber/reuse one: a stale flash record's values are read back
   positionally (see params.c), so reordering would silently reinterpret
   old data as the wrong param. */
typedef enum {
    PARAM_TEST_COUNTER = 0, /* throwaway, this issue's own proof -- remove once a
                                real param exists and nothing still depends on this
                                one for CLI/bench verification */
    PARAM_COUNT,
} ParamId;

typedef enum {
    PARAM_TYPE_U32,
} ParamType;

typedef struct {
    const char *name;
    ParamType type;
    uint32_t defaultValue;
} ParamDef;

/* One entry per ParamId, defined in params.c -- the CLI's `param list`
   walks this directly. */
extern ParamDef const g_paramDefs[PARAM_COUNT];

/* Loads every param from flash into the in-RAM cache get/set below read
   and write -- call once, before any param_get_u32()/param_set_u32()/CLI
   use. Falls back to every param's own default on a missing/invalid (bad
   magic/version/CRC) flash record -- same all-or-nothing validity check
   as aoa-boat-controller's *ConfigStore::load(), not a per-field
   fallback (see params.c). */
void params_init(void);

/* Returns false (out unchanged) if id is out of range. */
bool param_get_u32(ParamId id, uint32_t *outValue);

/* Updates the in-RAM cache and persists the whole record to flash
   immediately (not batched) -- see lib/params/<chip>.c's own comment for
   the per-call flash cost. Returns false (cache rolled back to its prior
   value, nothing written) if id is out of range OR the flash write
   itself failed -- callers must check this, not assume success. Issue
   #32's flash-safety requirement: the prior art this was ported from
   (aoa-boat-controller's ConfigFlash) never checked its underlying HAL
   erase/program return codes and kept going regardless, the suspected
   cause of a real "booted into DFU fine, failed to erase while
   flashing" bug there -- not repeating that here (see
   params_backend.h's own comment). */
bool param_set_u32(ParamId id, uint32_t value);

/* Returns -1 if no param with this exact name exists -- used by the
   CLI's `param show <name>`/`param set <name> <value>` to resolve a
   typed name into a ParamId. */
int16_t param_find_by_name(char const *name);

#endif /* HELM_PARAMS_H */
