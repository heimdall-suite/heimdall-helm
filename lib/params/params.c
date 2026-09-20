#include "params.h"
#include "params_backend.h"

#include <stddef.h>
#include <string.h>

/* Registry -- one entry per ParamId (params.h). Positional: index i here
   is PARAM_id's stored value at values[i] in ParamsRecord below, so
   entries must only ever be appended, never reordered (see params.h's
   own comment on PARAM_TEST_COUNTER). */
ParamDef const g_paramDefs[PARAM_COUNT] = {
    {"test_counter", PARAM_TYPE_U32, 0},
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
#define PARAMS_VERSION 1U

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
