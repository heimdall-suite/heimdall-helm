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

/* Issue #39 -- output.c's per-physical-slot trim (endpoints/
   subtrim/reverse) gets a fixed-size block of this many slots' worth of
   params, regardless of which board is actually compiling. 8 = the
   larger of the two real boards' own HELM_SERVO_COUNT (matek_h743) --
   sized to the max rather than each board's own HELM_SERVO_COUNT so
   this enum can stay a plain, positionally-stable, literally-named list
   (params.c's g_paramDefs needs real compile-time string literals for
   the CLI's `param list`/`param set <name>` -- no runtime string
   building exists in this store). Costs afroflight32 (HELM_SERVO_COUNT
   6) 2 slots' worth of unused params -- 32 bytes of RAM, negligible even
   against that board's own tight budget. output.c only ever reads
   indices below its own HELM_SERVO_COUNT; the rest just sit unused. */
#define HELM_PARAMS_MAX_OUTPUT_SLOTS 8

/* Which field within one output slot's 4-param group -- see
   param_output_slot_id() below. Order is fixed (matches the contiguous
   layout PARAM_OUTPUT_SLOT_BASE reserves in ParamId) -- don't reorder,
   same positional-storage discipline as ParamId itself. */
typedef enum {
    PARAM_OUTPUT_FIELD_MIN = 0,
    PARAM_OUTPUT_FIELD_CENTER,
    PARAM_OUTPUT_FIELD_MAX,
    PARAM_OUTPUT_FIELD_REVERSED,
    PARAM_OUTPUT_FIELD_COUNT,
} ParamOutputField;

/* Every persisted value gets a stable id here -- the registry/CLI index
   into this, not into raw byte offsets. Append new params at the end,
   never renumber/reuse one: a stale flash record's values are read back
   positionally (see params.c), so reordering would silently reinterpret
   old data as the wrong param. */
typedef enum {
    PARAM_TEST_COUNTER = 0, /* throwaway, this issue's own proof -- remove once a
                                real param exists and nothing still depends on this
                                one for CLI/bench verification */
    PARAM_INPUT_MODE,       /* issue #10 -- originally lib/rx/rx.c's runtime
                                protocol pick, RX_INPUT_MODE_SBUS/_CRSF (rx.h).
                                Superseded by PARAM_INPUT_PROTOCOL below as of
                                issue #55 -- rx.c now reads that instead (see
                                rx_start()'s own comment in rx.c). Kept defined
                                here, never renumbered/removed (this enum's own
                                positional-storage rule, this file's own top
                                comment), but nothing reads it any more; stays
                                purely as a historical/legacy slot. test_counter
                                above stays for the same never-remove reason. */
    PARAM_SERVO_RATE,       /* issue #31 -- lib/servo/servo.c's PWM frame rate,
                                one setting for every output slot (period is a
                                per-timer property, shared across every channel
                                on it -- not something that can differ per slot
                                within a timer group, let alone per board).
                                SERVO_RATE_50HZ/_333HZ (servo.h). */

    /* Issue #54 -- one .port/.protocol/.source triple per peripheral-
       routing subsystem (.docs/architecture/ports.md), the model that
       replaces the old one-bit-per-role HELM_HAS_GPS approach. Four real
       subsystems, not a throwaway proof value: gps/mag/input/telemetry
       are the ones ports.md itself names, validated through
       param_set_port() below, proven via the CLI same as every other
       param here. .protocol's meaning is entirely up to whichever driver
       reads it -- this store treats it as an opaque u32, no cross-param
       validation, same as PARAM_SERVO_RATE above.

       mag: no driver reads this yet (#57's job) -- pure bookkeeping.
       .port defaults to PARAM_PORT_UNSET and .source to
       PARAM_PORT_SOURCE_NONE, deliberately no forced claim on first
       boot, matching ports.md's "GPS/mag: always a runtime choice"
       stance (the exact conflation HELM_HAS_GPS got wrong).

       input/telemetry/gps: real first consumers, in that order --
       issue #55 made input/telemetry real (rx.c reads
       PARAM_INPUT_PROTOCOL, superseding PARAM_INPUT_MODE above), issue
       #56 did the same for gps (lib/sensors/gps.c, replacing the old
       HELM_HAS_GPS compile-time flag entirely). Unlike mag, all three
       get real per-board .port/.source defaults (params.c) matching
       this board's actual wiring, not an inert
       PARAM_PORT_UNSET/_SOURCE_NONE -- the "boot-identical" requirement
       #55/#56 both share means a freshly-flashed board still claims the
       same physical port its receiver/telemetry/GPS link already uses
       today. */
    PARAM_GPS_PORT,
    PARAM_GPS_PROTOCOL,
    PARAM_GPS_SOURCE,
    PARAM_MAG_PORT,
    PARAM_MAG_PROTOCOL,
    PARAM_MAG_SOURCE,
    PARAM_INPUT_PORT,
    PARAM_INPUT_PROTOCOL,
    PARAM_INPUT_SOURCE,
    PARAM_TELEMETRY_PORT,
    PARAM_TELEMETRY_PROTOCOL,
    PARAM_TELEMETRY_SOURCE,

    /* Issue #39 -- first of HELM_PARAMS_MAX_OUTPUT_SLOTS *
       PARAM_OUTPUT_FIELD_COUNT contiguous per-slot-per-field params;
       reach any of them via param_output_slot_id(), don't index this
       directly. */
    PARAM_OUTPUT_SLOT_BASE,

    PARAM_COUNT = PARAM_OUTPUT_SLOT_BASE + (HELM_PARAMS_MAX_OUTPUT_SLOTS * PARAM_OUTPUT_FIELD_COUNT),
} ParamId;

/* Which physical transport a `<subsystem>.port` claim needs -- selects
   whether param_set_port() checks HELM_HAS_PORT_<X>_UART or
   HELM_HAS_PORT_<X>_I2C (#53) for the port letter being set. The caller
   (today: the CLI's cmd_param(); eventually: whichever driver #55-57
   adds) supplies this, not something this store derives from
   `.protocol`'s value -- deriving it would mean this generic store
   knowing every subsystem's own protocol-to-transport mapping, which
   isn't a fact this store owns. */
typedef enum {
    PARAM_PORT_TRANSPORT_UART = 0,
    PARAM_PORT_TRANSPORT_I2C,
} ParamPortTransport;

/* `<subsystem>.source`'s value -- see ports.md's own "Onboard fact vs.
   runtime choice" section for the full model this mirrors. Only NONE/
   DIRECT/ONBOARD exist today; higher values are reserved for a future
   bridge subsystem's own index (e.g. sport_bridge, ports.md's "Telemetry
   bridges" section) -- not built by this issue, just a reserved value
   shape, per #54's own scope note. Whether ONBOARD is actually legal for
   a given subsystem on a given board (HELM_HAS_MAG etc., board_features.h)
   is NOT validated by this store -- deliberately out of scope, #54's
   own Validation section only covers `.port`, see param_set_port(). */
typedef enum {
    PARAM_PORT_SOURCE_NONE = 0,   /* not configured, no active source */
    PARAM_PORT_SOURCE_DIRECT,     /* uses this subsystem's own .port + .protocol */
    PARAM_PORT_SOURCE_ONBOARD,    /* fixed chip, legal only where board_features.h
                                      says it's actually there (unchecked here) */
} ParamPortSource;

/* Sentinel `.port` value meaning "this subsystem currently claims no
   port" -- distinct from every real port index (0-8, letters A-I, the
   largest span any of the three current boards uses). Ports are 0-based
   letter indices (A=0, B=1, ...), not raw HELM_HAS_PORT_<X>_* bit
   positions -- the CLI (lib/cli/cli.c) is where the letter<->index
   translation lives, this store only ever sees/stores the index, same
   "store a raw index, translate for display" choice #54's own Encoding
   section called out needing a decision. */
#define PARAM_PORT_UNSET 0xFFU

/* Resolves the ParamId for output slot `slot`'s `field` -- `slot` is
   output.c's own slotConfigs[] array index (0-based), NOT the physical
   silkscreen label (S3/OUT1/etc, output.c's own per-slot `.name`), since
   this enum is shared across boards whose silkscreen labels differ;
   cross-reference against output.c's own slotConfigs[] table to map
   index -> physical pad. `slot` must be < HELM_PARAMS_MAX_OUTPUT_SLOTS
   (the compile-time max above), NOT necessarily this board's own
   HELM_SERVO_COUNT, which may be smaller. */
static inline ParamId param_output_slot_id(uint8_t slot, ParamOutputField field) {
    return (ParamId)(PARAM_OUTPUT_SLOT_BASE + ((uint32_t)slot * PARAM_OUTPUT_FIELD_COUNT) + (uint32_t)field);
}

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

/* True if `id` is one of the 4 subsystems' `.port` fields above (#54) --
   used by the CLI to decide whether a value should print/parse as a
   letter instead of a raw decimal number. Everything else in this store
   (including `.protocol`/`.source`, and every param that predates #54)
   stays plain PARAM_TYPE_U32 decimal, same as today. */
bool param_id_is_port_field(ParamId id);

/* Does port letter `portIndex` (0-based, A=0) physically exist on this
   board and support `transport` -- checks board_features.h's
   HELM_HAS_PORT_<X>_UART/_I2C flags (#53). A board that never #defines
   the flag for a given letter (because that letter isn't in its own
   port inventory at all, .docs/hardware.md) is treated identically to
   the flag being defined 0 -- both mean "doesn't exist here". */
bool param_port_exists(uint8_t portIndex, ParamPortTransport transport);

/* Sets a `<subsystem>.port` field (`portId`, one of the 12 PARAM_*_PORT
   ids above) to `portIndex`, after #54's two required checks: (1) does
   this port exist on this board at all for `transport`
   (param_port_exists() above) and (2) is it already claimed -- currently
   `.source == PARAM_PORT_SOURCE_DIRECT` -- by a DIFFERENT subsystem.
   Rejects (returns false, nothing written/persisted) if either check
   fails, same all-or-nothing contract as param_set_u32(); also returns
   false if the underlying param_set_u32() call itself fails (flash write
   error). This is the only validated setter in this store -- every other
   param (including this same subsystem's own `.protocol`/`.source`)
   still goes through plain param_set_u32(), unvalidated, same as before
   this issue. */
bool param_set_port(ParamId portId, uint8_t portIndex, ParamPortTransport transport);

#endif /* HELM_PARAMS_H */
