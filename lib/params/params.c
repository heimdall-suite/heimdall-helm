#include "params.h"
#include "params_backend.h"

#include "board_features.h"

#include <stddef.h>
#include <string.h>

/* Registry -- one entry per ParamId (params.h). Positional: index i here
   is PARAM_id's stored value at values[i] in ParamsRecord below, so
   entries must only ever be appended, never reordered (see params.h's
   own comment on PARAM_TEST_COUNTER).

   input_mode's default is deliberately this board's own compile-time
   HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF (board_features.h), not a bare 0 --
   a never-written/freshly-erased record (params_init()'s "invalid,
   fall back to defaults" path) must still boot into the protocol this
   board's receiver is actually wired for (issue #10), not silently
   default to SBUS on a board wired for CRSF. Raw 0U/1U here, NOT
   lib/rx/rx.h's RX_INPUT_MODE_SBUS/_CRSF -- this file is built via
   add_params.py's own explicit BuildSources() call (see that script's
   comment), which never runs PlatformIO's LDF chain-scan over it, so it
   has no include path to lib/rx/ at all. Values must stay in sync with
   rx.h's own by hand; a board's HELM_RX_DEFAULT_PROTOCOL_* pick already
   works the same duplicated-by-hand way across board_features.h files. */
ParamDef const g_paramDefs[PARAM_COUNT] = {
    {"test_counter", PARAM_TYPE_U32, 0},
    {"input_mode", PARAM_TYPE_U32, HELM_RX_DEFAULT_PROTOCOL_CRSF ? 1U : 0U},
    /* servo_rate's default is 0 (SERVO_RATE_50HZ, lib/servo/servo.h) on
       every board -- unlike input_mode, this isn't a per-board hardware
       fact (no board_features.h flag drives it): 50Hz is simply the safe
       universal default that works with any servo, chosen once here for
       every target rather than duplicated per board_features.h. Raw 0U,
       not servo.h's SERVO_RATE_50HZ, for the same reason input_mode uses
       raw 0U/1U above -- no include path to lib/servo/ from this file. */
    {"servo_rate", PARAM_TYPE_U32, 0U},

    /* Issue #54 -- gps/mag/input/telemetry's own .port/.protocol/.source
       triples (params.h's own comment on the PARAM_GPS_PORT block has
       the full rationale). PARAM_PORT_UNSET (0xFF), not 0 -- 0 is Port A,
       a real port, so it can't double as "no port claimed" the way
       out0.min etc. use a real 0 default elsewhere in this file.
       PARAM_PORT_SOURCE_NONE (0) is the real "not configured" value for
       .source, no clash there. .protocol defaults to plain 0 -- opaque,
       no meaning assigned by this store (params.h's own comment).

       gps/mag stay fully inert here -- #54's original "no forced claim"
       defaults, unchanged. No driver reads them yet (#56/#57's job). */
    {"gps.port", PARAM_TYPE_U32, PARAM_PORT_UNSET},
    {"gps.protocol", PARAM_TYPE_U32, 0U},
    {"gps.source", PARAM_TYPE_U32, PARAM_PORT_SOURCE_NONE},
    {"mag.port", PARAM_TYPE_U32, PARAM_PORT_UNSET},
    {"mag.protocol", PARAM_TYPE_U32, 0U},
    {"mag.source", PARAM_TYPE_U32, PARAM_PORT_SOURCE_NONE},

    /* Issue #55 -- input/telemetry are the first real consumers of #54's
       shape, so unlike gps/mag above, these need real per-board defaults,
       not an inert PARAM_PORT_UNSET -- #55's own "boot-identical"
       requirement means a freshly-flashed/never-written board must still
       claim the exact port its receiver/telemetry link is actually wired
       to (.docs/hardware.md's port inventory, #52), same "don't silently
       default away from this board's real wiring" reasoning input_mode's
       own comment above already gives. Keyed off the STM32H7/STM32F1/
       STM32F7 chip-family macros lib/cli/cli.c already uses for board
       dispatch in a shared file -- this file has no include path to
       board.h's HELM_BOARD_NAME (same BuildSources() limitation this
       file's own top comment already explains for lib/rx/), so a
       preprocessor macro already available in every translation unit is
       the only board-identifying signal available here. One #define
       block per board, same "duplicated by hand" convention this file's
       own input_mode comment already documents for
       HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF. */
#if defined(STM32H7) /* matek_h743 */
#define HELM_INPUT_DEFAULT_PORT 4U         /* Port E, UART6 -- SBUS today (#8) */
#define HELM_TELEMETRY_DEFAULT_PORT 5U     /* Port F, UART7 -- S.Port today (#18) */
#define HELM_TELEMETRY_DEFAULT_PROTOCOL 0U /* the only telemetry protocol implemented
                                               today (sport.c) -- no public enum yet,
                                               nothing branches on this value (#55's own
                                               scope: prove the mechanism against
                                               already-shipped wiring, not build real
                                               telemetry-protocol dispatch) */
#elif defined(STM32F1)                            /* afroflight32 */
#define HELM_INPUT_DEFAULT_PORT 0U                /* Port A, USART2 -- SBUS today (#52's audit) */
#define HELM_TELEMETRY_DEFAULT_PORT PARAM_PORT_UNSET /* no S.Port UART on this board
                                                          (HELM_HAS_SPORT_UART 0) */
#define HELM_TELEMETRY_DEFAULT_PROTOCOL 0U
#else /* nexus_xr (STM32F7) -- onboard ExpressLRS receiver (UART5, not a
         connector), ports.md's `source = onboard` case, not `direct` --
         .port stays unset, same as this board keeps every other
         HELM_HAS_* flag at 0: no bench-confirmed hardware to claim a
         real port index against yet. This file is excluded from
         nexus_xr's build entirely today anyway
         (HELM_FEATURE_PARAMS_PERSIST 0, board_features.h) -- this
         branch only matters once that changes. */
#define HELM_INPUT_DEFAULT_PORT PARAM_PORT_UNSET
#define HELM_TELEMETRY_DEFAULT_PORT PARAM_PORT_UNSET
#define HELM_TELEMETRY_DEFAULT_PROTOCOL 0U
#endif

#if defined(STM32F7) /* nexus_xr -- see the #define block above */
#define HELM_INPUT_DEFAULT_SOURCE PARAM_PORT_SOURCE_ONBOARD
#else /* matek_h743/afroflight32 -- both wired directly, no onboard/bridge case for RX today */
#define HELM_INPUT_DEFAULT_SOURCE PARAM_PORT_SOURCE_DIRECT
#endif

#if defined(STM32H7) /* matek_h743 -- the only board with a real S.Port UART today */
#define HELM_TELEMETRY_DEFAULT_SOURCE PARAM_PORT_SOURCE_DIRECT
#else
#define HELM_TELEMETRY_DEFAULT_SOURCE PARAM_PORT_SOURCE_NONE
#endif

    {"input.port", PARAM_TYPE_U32, HELM_INPUT_DEFAULT_PORT},
    {"input.protocol", PARAM_TYPE_U32, HELM_RX_DEFAULT_PROTOCOL_CRSF ? 1U : 0U}, /* same
                                                        raw-0U/1U-not-rx.h's-enum reasoning
                                                        as input_mode's default above --
                                                        this is input_mode's real successor
                                                        as of #55 (see rx.c) */
    {"input.source", PARAM_TYPE_U32, HELM_INPUT_DEFAULT_SOURCE},
    {"telemetry.port", PARAM_TYPE_U32, HELM_TELEMETRY_DEFAULT_PORT},
    {"telemetry.protocol", PARAM_TYPE_U32, HELM_TELEMETRY_DEFAULT_PROTOCOL},
    {"telemetry.source", PARAM_TYPE_U32, HELM_TELEMETRY_DEFAULT_SOURCE},

    /* Issue #39 -- output.c's per-slot endpoint/subtrim/reverse
       trim, HELM_PARAMS_MAX_OUTPUT_SLOTS (params.h) slots' worth
       regardless of board (see that file's own comment on why this
       isn't sized per-board). Defaults reproduce output.c's own
       pre-#39 hardcoded slotConfigs[] table exactly -- both real boards'
       tables agree on these same numbers for every slot (S3/OUT1 gets
       the asymmetric reversed 1000/1550/2000 subtrim, every other slot
       gets the plain symmetric 1000/1500/2000) -- so a first boot after
       #39 lands behaves identically to before it, no behavior change
       until someone actually calls `param set`. Raw 1000/1500/1550/2000
       here, not output.c's own OUTPUT_SERVO_MIN_US/_CENTER_US/_MAX_US --
       this file has no include path to output.c's private #defines
       (same "duplicated by hand, kept in sync manually" convention this
       file's own input_mode comment already documents for
       HELM_RX_DEFAULT_PROTOCOL_SBUS/_CRSF).
       Names are out<N>.min/center/max/reversed, N = output.c's own
       slotConfigs[] array index (0-based, NOT the physical silkscreen
       label -- see param_output_slot_id()'s own comment in params.h). */
    {"out0.min", PARAM_TYPE_U32, 1000U},
    {"out0.center", PARAM_TYPE_U32, 1550U},
    {"out0.max", PARAM_TYPE_U32, 2000U},
    {"out0.reversed", PARAM_TYPE_U32, 1U},
    {"out1.min", PARAM_TYPE_U32, 1000U},
    {"out1.center", PARAM_TYPE_U32, 1500U},
    {"out1.max", PARAM_TYPE_U32, 2000U},
    {"out1.reversed", PARAM_TYPE_U32, 0U},
    {"out2.min", PARAM_TYPE_U32, 1000U},
    {"out2.center", PARAM_TYPE_U32, 1500U},
    {"out2.max", PARAM_TYPE_U32, 2000U},
    {"out2.reversed", PARAM_TYPE_U32, 0U},
    {"out3.min", PARAM_TYPE_U32, 1000U},
    {"out3.center", PARAM_TYPE_U32, 1500U},
    {"out3.max", PARAM_TYPE_U32, 2000U},
    {"out3.reversed", PARAM_TYPE_U32, 0U},
    {"out4.min", PARAM_TYPE_U32, 1000U},
    {"out4.center", PARAM_TYPE_U32, 1500U},
    {"out4.max", PARAM_TYPE_U32, 2000U},
    {"out4.reversed", PARAM_TYPE_U32, 0U},
    {"out5.min", PARAM_TYPE_U32, 1000U},
    {"out5.center", PARAM_TYPE_U32, 1500U},
    {"out5.max", PARAM_TYPE_U32, 2000U},
    {"out5.reversed", PARAM_TYPE_U32, 0U},
    /* out6/out7: only meaningful on matek_h743 (HELM_SERVO_COUNT 8) --
       afroflight32 (HELM_SERVO_COUNT 6) never reads these, see
       HELM_PARAMS_MAX_OUTPUT_SLOTS's own comment in params.h. Still need
       real default values, even if unread there -- 0 would be a nonsense
       PWM pulse width, so these carry the same plain-slot default as
       every other non-S3/OUT1 slot, not a bare 0. */
    {"out6.min", PARAM_TYPE_U32, 1000U},
    {"out6.center", PARAM_TYPE_U32, 1500U},
    {"out6.max", PARAM_TYPE_U32, 2000U},
    {"out6.reversed", PARAM_TYPE_U32, 0U},
    {"out7.min", PARAM_TYPE_U32, 1000U},
    {"out7.center", PARAM_TYPE_U32, 1500U},
    {"out7.max", PARAM_TYPE_U32, 2000U},
    {"out7.reversed", PARAM_TYPE_U32, 0U},
};

/* magic+version+CRC-validated record, same convention as
   aoa-boat-controller's *ConfigStore classes (params.h's own comment) --
   an invalid or never-written record falls back to every param's
   compiled default, not a per-field fallback. Bump kVersion, not
   kMagic, whenever this shape changes (field added/resized/reordered) so
   a stale record under the old layout is detected and discarded rather
   than misread -- same convention that codebase's UserPinConfig::kVersion
   comment documents. */
#define PARAMS_MAGIC 0xA5U
/* Bumped 1->2 for issue #10 (adding PARAM_INPUT_MODE grew values[] from 1
   to 2 u32s -- see git history for the full account of the bug this
   caught: a stale 1-value record misread as 2-value, input_mode reading
   back as 233 instead of falling back to its default). Bumped 2->3 for
   issue #31 (adding PARAM_SERVO_RATE grows values[] again, 2->3 u32s) --
   same shape change, same reasoning, applied proactively this time
   rather than caught by a repeat of that same bug. Bumped 3->4 for issue
   #39 (adding the 32 output-slot-trim params grows values[] from
   3 to 35 u32s) -- same shape change again, same reasoning: a record
   saved under version 3 must be discarded, not misread with 32 slot
   params reinterpreted from bytes that were never written for them.
   Bumped 4->5 for issue #54 (adding the 12 gps/mag/input/telemetry
   port/protocol/source params grows values[] from 35 to 47 u32s) --
   same shape change, same reasoning.
   Bumped 5->6 for issue #55 -- NOT a values[] size change (still 47
   u32s), but the same underlying hazard this whole scheme exists to
   catch: input.port/input.source's meaning changed from "always inert"
   (#54's PARAM_PORT_UNSET/_SOURCE_NONE) to "must reflect this board's
   real wiring" (#55's per-board defaults), and any record already
   persisted under version 5 -- even one written by an unrelated `param
   set` that happened to touch a totally different param, since every
   write persists the whole record -- would silently keep the old inert
   values forever, masking #54's collision check exactly the way a
   stale flash record silently defeated the version-1 bug above.
   Bench-caught on this project's own real matek_h743 unit while
   verifying #55: `telemetry.port e` wrongly succeeded because a record
   persisted during #54's own bench session still held input.port as
   PARAM_PORT_UNSET, so nothing was there to collide with. Discarding
   old records here, not just relying on "freshly-flashed" as #55's own
   boot-identical requirement assumed, closes that gap for real. */
#define PARAMS_VERSION 6U

typedef struct {
    uint8_t magic;
    uint8_t version;
    uint32_t values[PARAM_COUNT];
    uint8_t crc;
} ParamsRecord;

_Static_assert(sizeof(ParamsRecord) <= PARAMS_REGION_SIZE,
               "ParamsRecord has outgrown PARAMS_REGION_SIZE -- grow the backend's reserved region");

/* Bytes the CRC actually covers: everything BEFORE the crc field.
   Deliberately offsetof(..., crc), NOT sizeof(record)-sizeof(record.crc)
   -- the latter is only correct if crc is the struct's last byte with no
   padding after it, true of aoa-boat-controller's *ConfigStore records
   (every field there is uint8_t, so nothing forces alignment padding)
   but NOT true here: `values` is uint32_t, so the compiler inserts 2
   padding bytes after magic/version to align it, and crc's own 1 byte
   leaves 3 more trailing padding bytes before the struct's size rounds
   up to a multiple of 4. sizeof(record)-sizeof(record.crc) (11) would
   therefore span bytes [0,11) -- which includes crc's own offset (8)
   and two of those trailing padding bytes, making the checksum
   self-referential and non-deterministic (crc computed over its own
   not-yet-assigned byte). Bench-caught on real hardware (issue #32):
   builds and single-session get/set round-trips looked correct --
   param_get_u32() only ever reads the RAM cache -- but a value never
   survived an actual reflash, since params_init()'s from-flash CRC
   re-check is the only code path that exercises this range at all. */
#define PARAMS_CRC_LEN offsetof(ParamsRecord, crc)

static uint32_t s_cache[PARAM_COUNT];

/* Simple CRC-8 (polynomial 0x07) -- same algorithm aoa-boat-controller's
   *ConfigStore records use, picked here for the same reason: cheap, and
   this store's own record layout is directly comparable to that prior
   art's, worth being able to reason about the same way. */
static uint8_t crc8(uint8_t const *data, uint32_t len) {
    uint8_t crc = 0;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x07U) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static void load_defaults(void) {
    for (uint16_t i = 0; i < PARAM_COUNT; i++) {
        s_cache[i] = g_paramDefs[i].defaultValue;
    }
}

void params_init(void) {
    ParamsRecord record;
    params_backend_read(0, &record, sizeof(record));

    uint8_t const expectedCrc = crc8((uint8_t const *)&record, PARAMS_CRC_LEN);
    bool const valid = record.magic == PARAMS_MAGIC && record.version == PARAMS_VERSION && record.crc == expectedCrc;

    if (!valid) {
        /* Also the honest outcome on a never-written/freshly-erased
           region (reads back as 0xFF bytes on both chip families this
           project targets) -- not a fault, just "nothing saved yet". */
        load_defaults();
        return;
    }

    memcpy(s_cache, record.values, sizeof(s_cache));
}

bool param_get_u32(ParamId id, uint32_t *outValue) {
    if (id >= PARAM_COUNT) {
        return false;
    }
    *outValue = s_cache[id];
    return true;
}

bool param_set_u32(ParamId id, uint32_t value) {
    if (id >= PARAM_COUNT) {
        return false;
    }

    uint32_t const previous = s_cache[id];
    s_cache[id] = value;

    /* Zero-initialized, not left uninitialized -- magic/version/values
       don't cover every byte PARAMS_CRC_LEN checksums (the 2 padding
       bytes the compiler inserts before `values` to satisfy its uint32_t
       alignment are never explicitly assigned), and this struct gets
       flashed byte-for-byte. Deterministic padding costs nothing here
       and removes any dependence on incidental stack contents. */
    ParamsRecord record = {0};
    record.magic = PARAMS_MAGIC;
    record.version = PARAMS_VERSION;
    memcpy(record.values, s_cache, sizeof(s_cache));
    record.crc = crc8((uint8_t const *)&record, PARAMS_CRC_LEN);

    if (!params_backend_write(0, &record, sizeof(record))) {
        /* Roll the cache back -- a failed flash write must not leave the
           in-RAM value claiming success (params_backend_write()'s own
           comment: this is the exact failure mode issue #32 exists to
           close off). */
        s_cache[id] = previous;
        return false;
    }

    return true;
}

int16_t param_find_by_name(char const *name) {
    for (uint16_t i = 0; i < PARAM_COUNT; i++) {
        if (strcmp(g_paramDefs[i].name, name) == 0) {
            return (int16_t)i;
        }
    }
    return -1;
}

bool param_id_is_port_field(ParamId id) {
    return id == PARAM_GPS_PORT || id == PARAM_MAG_PORT || id == PARAM_INPUT_PORT ||
           id == PARAM_TELEMETRY_PORT;
}

/* Port-existence lookup (issue #54, validating against #53's per-board
   HELM_HAS_PORT_<X>_UART/_I2C flags). Guarded per letter with #ifdef,
   not a bare HELM_HAS_PORT_<X>_UART reference -- a board only ever
   #defines the flag for port letters its own inventory
   (.docs/hardware.md) actually names; afroflight32, for example, never
   defines HELM_HAS_PORT_C_UART at all (it only has Ports A/B), so
   referencing it directly would fail to compile there. An undefined
   flag here is treated identically to one #defined 0 -- both mean
   "this board doesn't have this port letter/transport". 9 letters
   (A-I) -- the largest port-letter span any of the three current
   boards uses (matek_h743's I) -- bump if a future board ever needs
   more. */
#define HELM_PORT_LETTER_COUNT 9U

#if defined(HELM_HAS_PORT_A_UART) && HELM_HAS_PORT_A_UART
#define HELM_PORT_A_UART_BIT 1U
#else
#define HELM_PORT_A_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_B_UART) && HELM_HAS_PORT_B_UART
#define HELM_PORT_B_UART_BIT 1U
#else
#define HELM_PORT_B_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_C_UART) && HELM_HAS_PORT_C_UART
#define HELM_PORT_C_UART_BIT 1U
#else
#define HELM_PORT_C_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_D_UART) && HELM_HAS_PORT_D_UART
#define HELM_PORT_D_UART_BIT 1U
#else
#define HELM_PORT_D_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_E_UART) && HELM_HAS_PORT_E_UART
#define HELM_PORT_E_UART_BIT 1U
#else
#define HELM_PORT_E_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_F_UART) && HELM_HAS_PORT_F_UART
#define HELM_PORT_F_UART_BIT 1U
#else
#define HELM_PORT_F_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_G_UART) && HELM_HAS_PORT_G_UART
#define HELM_PORT_G_UART_BIT 1U
#else
#define HELM_PORT_G_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_H_UART) && HELM_HAS_PORT_H_UART
#define HELM_PORT_H_UART_BIT 1U
#else
#define HELM_PORT_H_UART_BIT 0U
#endif
#if defined(HELM_HAS_PORT_I_UART) && HELM_HAS_PORT_I_UART
#define HELM_PORT_I_UART_BIT 1U
#else
#define HELM_PORT_I_UART_BIT 0U
#endif

#if defined(HELM_HAS_PORT_A_I2C) && HELM_HAS_PORT_A_I2C
#define HELM_PORT_A_I2C_BIT 1U
#else
#define HELM_PORT_A_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_B_I2C) && HELM_HAS_PORT_B_I2C
#define HELM_PORT_B_I2C_BIT 1U
#else
#define HELM_PORT_B_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_C_I2C) && HELM_HAS_PORT_C_I2C
#define HELM_PORT_C_I2C_BIT 1U
#else
#define HELM_PORT_C_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_D_I2C) && HELM_HAS_PORT_D_I2C
#define HELM_PORT_D_I2C_BIT 1U
#else
#define HELM_PORT_D_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_E_I2C) && HELM_HAS_PORT_E_I2C
#define HELM_PORT_E_I2C_BIT 1U
#else
#define HELM_PORT_E_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_F_I2C) && HELM_HAS_PORT_F_I2C
#define HELM_PORT_F_I2C_BIT 1U
#else
#define HELM_PORT_F_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_G_I2C) && HELM_HAS_PORT_G_I2C
#define HELM_PORT_G_I2C_BIT 1U
#else
#define HELM_PORT_G_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_H_I2C) && HELM_HAS_PORT_H_I2C
#define HELM_PORT_H_I2C_BIT 1U
#else
#define HELM_PORT_H_I2C_BIT 0U
#endif
#if defined(HELM_HAS_PORT_I_I2C) && HELM_HAS_PORT_I_I2C
#define HELM_PORT_I_I2C_BIT 1U
#else
#define HELM_PORT_I_I2C_BIT 0U
#endif

static uint8_t const kPortUartExists[HELM_PORT_LETTER_COUNT] = {
    HELM_PORT_A_UART_BIT, HELM_PORT_B_UART_BIT, HELM_PORT_C_UART_BIT, HELM_PORT_D_UART_BIT,
    HELM_PORT_E_UART_BIT, HELM_PORT_F_UART_BIT, HELM_PORT_G_UART_BIT, HELM_PORT_H_UART_BIT,
    HELM_PORT_I_UART_BIT,
};
static uint8_t const kPortI2cExists[HELM_PORT_LETTER_COUNT] = {
    HELM_PORT_A_I2C_BIT, HELM_PORT_B_I2C_BIT, HELM_PORT_C_I2C_BIT, HELM_PORT_D_I2C_BIT,
    HELM_PORT_E_I2C_BIT, HELM_PORT_F_I2C_BIT, HELM_PORT_G_I2C_BIT, HELM_PORT_H_I2C_BIT,
    HELM_PORT_I_I2C_BIT,
};

bool param_port_exists(uint8_t portIndex, ParamPortTransport transport) {
    if (portIndex >= HELM_PORT_LETTER_COUNT) {
        return false;
    }
    return (transport == PARAM_PORT_TRANSPORT_UART ? kPortUartExists[portIndex]
                                                    : kPortI2cExists[portIndex]) != 0U;
}

/* The one place this otherwise subsystem-agnostic store knows the 4
   real subsystems' names -- same kind of necessary exception g_paramDefs
   itself already is. Whichever subsystem is added next (a future bridge,
   ports.md's "Telemetry bridges" section) gets a row here too, same as
   it gets rows in g_paramDefs above. */
typedef struct {
    ParamId portId;
    ParamId sourceId;
} PortOwnerFields;

static PortOwnerFields const kPortOwners[] = {
    {PARAM_GPS_PORT, PARAM_GPS_SOURCE},
    {PARAM_MAG_PORT, PARAM_MAG_SOURCE},
    {PARAM_INPUT_PORT, PARAM_INPUT_SOURCE},
    {PARAM_TELEMETRY_PORT, PARAM_TELEMETRY_SOURCE},
};

bool param_set_port(ParamId portId, uint8_t portIndex, ParamPortTransport transport) {
    if (portIndex != PARAM_PORT_UNSET && !param_port_exists(portIndex, transport)) {
        return false;
    }

    /* Collision check -- a port claimed (`.source == DIRECT`) by a
       DIFFERENT subsystem already pointing its own `.port` at the same
       letter is rejected, full stop (ports.md's port-collision rule).
       A subsystem whose `.source != DIRECT` doesn't count as claiming
       anything, regardless of whatever its `.port` currently holds --
       `.port`/`.protocol` are only meaningful when `.source == DIRECT`
       (params.h's own comment), so a stale/default `.port` value on an
       onboard- or none-sourced subsystem must never block someone else
       from taking that letter. PARAM_PORT_UNSET itself never collides
       with anything, real or not -- skip the check entirely when
       clearing a claim. */
    if (portIndex != PARAM_PORT_UNSET) {
        for (size_t i = 0; i < (sizeof(kPortOwners) / sizeof(kPortOwners[0])); i++) {
            if (kPortOwners[i].portId == portId) {
                continue; /* self */
            }

            uint32_t otherSource = PARAM_PORT_SOURCE_NONE;
            uint32_t otherPort = PARAM_PORT_UNSET;
            param_get_u32(kPortOwners[i].sourceId, &otherSource);
            param_get_u32(kPortOwners[i].portId, &otherPort);

            if (otherSource == PARAM_PORT_SOURCE_DIRECT && otherPort == (uint32_t)portIndex) {
                return false;
            }
        }
    }

    return param_set_u32(portId, portIndex);
}
