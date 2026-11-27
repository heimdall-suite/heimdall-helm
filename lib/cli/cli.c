#include "board_features.h"

#if HELM_FEATURE_CLI

#include "cli.h"

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "diag.h"
#include "shell.h"
#include "usb_cdc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HELM_HAS_ROM_BOOTLOADER_JUMP
#include "bootloader.h"
#endif
#if HELM_FEATURE_PARAMS_PERSIST
#include "output.h"
#include "params.h"
#endif

#if defined(STM32H7)
#include "stm32h7xx_hal.h"
#elif defined(STM32F7)
#include "stm32f7xx_hal.h"
#elif defined(STM32F1)
#include "stm32f1xx_hal.h"
#else
#error "Unknown MCU family -- add its HAL include here for this board."
#endif

/* `status` command handler: prints the board name and current uptime.
   The first real command, added to prove the CLI round-trips. */
static void cmd_status(const char *args) {
    (void)args;
    char line[64];
    snprintf(line, sizeof(line), "board: %s  uptime: %lu ms\r\n", HELM_BOARD_NAME,
              (unsigned long)HAL_GetTick());
    shell_print(line);
}

#if HELM_HAS_ROM_BOOTLOADER_JUMP
/* `dfu` command handler: reboots straight into this chip's ROM
   bootloader over the same cable the CLI itself runs over -- the reason
   this feature was built in the first place, closing the loop with
   lib/bootloader so no BOOT-button/ST-Link is needed to reflash. Not
   necessarily USB DFU class specifically, despite the command's name
   (kept for muscle-memory/consistency across boards) -- matek_h743's is
   real USB DFU (lib/bootloader/stm32h7.c), afroflight32's (issue #28,
   lib/bootloader/stm32f1.c) is the chip's plain UART bootloader (AN3155)
   instead, reached over the same USART1 the CLI already runs over via
   the onboard USB-serial converter. Gated on HELM_HAS_ROM_BOOTLOADER_JUMP,
   not just HELM_FEATURE_CLI (issue #13) -- a board can have a CLI
   without a ported bootloader-jump (nexus_xr has neither at all; see
   board_features.h), so this would fail to link if registered
   unconditionally. */
static void cmd_dfu(const char *args) {
    (void)args;
    shell_print("rebooting into ROM bootloader...\r\n");
    bootloader_request_dfu();
}
#endif

#if HELM_FEATURE_PARAMS_PERSIST
/* `param` command handler: list/show/set over the generic persisted-
   param store (issue #32). A real operational command, not a bench-only
   diagnostic (see .docs/cli.md's "Adding a command" section), so it
   lives here directly rather than as a lib/diag subcommand. Only
   PARAM_TYPE_U32 exists today (params.h) -- the %lu formatting/strtoul
   parsing below is correct for every param except the 4 `.port` fields
   issue #54 added, which this file special-cases by ParamId (letter
   encoding, param_id_is_port_field() below) rather than by a second
   ParamType -- see params.h's own PARAM_PORT_UNSET comment for why the
   store itself still only ever sees a raw index. A real second
   ParamType would still need proper per-type dispatch here, not just a
   wider printf. */
static void param_print_value(ParamId id) {
    uint32_t value = 0;
    param_get_u32(id, &value); /* id always in range here -- every call site below already checked */
    char line[80];

    /* Issue #54 -- `.port` fields print as a letter (or "none" for
       PARAM_PORT_UNSET), not a raw index -- params.h's own comment on
       PARAM_PORT_UNSET explains why this store keeps the raw index and
       leaves letter<->index translation to this layer. Every other
       param, including this same subsystem's own `.protocol`/`.source`,
       stays plain decimal, same as before this issue. */
    if (param_id_is_port_field(id)) {
        if (value == PARAM_PORT_UNSET) {
            snprintf(line, sizeof(line), "%s = none\r\n", g_paramDefs[id].name);
        } else {
            snprintf(line, sizeof(line), "%s = %c\r\n", g_paramDefs[id].name, (char)('a' + value));
        }
        shell_print(line);
        return;
    }

    snprintf(line, sizeof(line), "%s = %lu\r\n", g_paramDefs[id].name, (unsigned long)value);
    shell_print(line);
}

/* Which physical transport a `.port` field needs (params.h's own
   ParamPortTransport comment explains why the store itself doesn't
   derive this from `.protocol`). A real hardware fact, not a param-store
   concern: mag is this project's one I2C-based subsystem (matek_h743's
   Port H, afroflight32's Port B -- both I2C compass buses,
   .docs/hardware.md); gps/input/telemetry are all serial protocols
   (NMEA/UBX, SBUS/CRSF, S.Port/CRSF) over a UART, same as every port
   they claim on real hardware today. Whichever driver (#55-57)
   eventually calls param_set_port() directly instead of going through
   this CLI owns this same fact itself at that point -- it doesn't move
   into lib/params/. */
static ParamPortTransport param_port_field_transport(ParamId id) {
    return id == PARAM_MAG_PORT ? PARAM_PORT_TRANSPORT_I2C : PARAM_PORT_TRANSPORT_UART;
}

/* Parses a `.port` field's CLI value: a single letter (a-i, case-
   insensitive) or the literal "none" to clear the claim
   (PARAM_PORT_UNSET) -- issue #54's own Encoding section calls for
   human-readable letters, not a raw index, on the input side too.
   Returns false (outValue unchanged) on anything else -- multi-
   character strings, digits, punctuation. */
static bool param_parse_port_value(char const *str, uint32_t *outValue) {
    if (strcmp(str, "none") == 0) {
        *outValue = PARAM_PORT_UNSET;
        return true;
    }
    if (strlen(str) == 1 && isalpha((unsigned char)str[0])) {
        *outValue = (uint32_t)(tolower((unsigned char)str[0]) - 'a');
        return true;
    }
    return false;
}

/* Issue #69 -- the one validated write path, shared by the human `param
   set` and the machine `cfg set`, so both reject exactly the same values
   with exactly the same reasons. Returns NULL on success, else a reason
   without any "REJECTED --" prefix; *writeFailed tells a flash failure
   (valid value, not saved) apart from a rejected value. `reason` is
   caller-provided scratch for formatted reasons -- on the CLI task's
   stack, not a static, so afroflight32 pays no RAM for it. */
static const char *param_apply(ParamId id, char const *valueStr, bool *writeFailed, char *reason,
                               size_t reasonSize) {
    *writeFailed = false;

    /* Issue #54 -- `.port` fields go through the validated setter
       (board-existence + collision checks, params.h's own
       param_set_port() comment). */
    if (param_id_is_port_field(id)) {
        uint32_t portValue = 0;
        if (!param_parse_port_value(valueStr, &portValue)) {
            return "expected a port letter (a-i) or \"none\"";
        }
        ParamPortTransport const transport = param_port_field_transport(id);
        if (portValue != PARAM_PORT_UNSET && !param_port_exists((uint8_t)portValue, transport)) {
            return "no such port on this board";
        }
        if (!param_set_port(id, (uint8_t)portValue, transport)) {
            return "already claimed by another subsystem, or flash write error";
        }
        return NULL;
    }

    /* Strict parse -- strtoul alone silently turned "abc" into 0. */
    char *end = NULL;
    uint32_t const value = (uint32_t)strtoul(valueStr, &end, 0);
    if (end == valueStr || *end != '\0' || valueStr[0] == '-') {
        return "not a number";
    }

    ParamDef const *def = &g_paramDefs[id];
    switch (param_value_allowed(id, value)) {
    case PARAM_CHECK_OK:
        break;
    case PARAM_CHECK_RANGE:
        snprintf(reason, reasonSize, "must be %lu-%lu%s", (unsigned long)def->min, (unsigned long)def->max,
                 def->unit == PARAM_UNIT_US ? "us" : "");
        return reason;
    case PARAM_CHECK_BOOL:
        return "must be 0 or 1";
    case PARAM_CHECK_CHOICE:
        snprintf(reason, reasonSize, "must be one of %s", def->choices);
        return reason;
    }

    /* #65 -- out<N>.* trim also needs the cross-field order check. */
    if (param_id_is_output_field(id)) {
        switch (param_set_output(id, value)) {
        case PARAM_OUTPUT_OK:
            return NULL;
        case PARAM_OUTPUT_REJECT_RANGE:
            return "pulse must be 500-2500us";
        case PARAM_OUTPUT_REJECT_ORDER:
            return "would break min <= center <= max for this slot (move the limit on the far side first)";
        case PARAM_OUTPUT_REJECT_BOOL:
            return "reversed must be 0 or 1";
        case PARAM_OUTPUT_REJECT_WRITE:
            break;
        }
        *writeFailed = true;
        return "flash write error, value NOT saved";
    }

    if (!param_set_u32(id, value)) {
        *writeFailed = true;
        return "flash write error, value NOT saved";
    }
    return NULL;
}

static void cmd_param(const char *args) {
    if (strcmp(args, "list") == 0) {
        for (uint16_t i = 0; i < PARAM_COUNT; i++) {
            param_print_value((ParamId)i);
        }
        return;
    }

    /* shell_register()'s ShellCommandHandler hands us a const pointer
       into the shell's own line buffer -- copy before tokenizing rather
       than casting away const to mutate it in place. */
    char buf[SHELL_MAX_LINE_LEN];
    strncpy(buf, args, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *verb = strtok(buf, " ");
    char *name = strtok(NULL, " ");
    char *valueStr = strtok(NULL, " ");

    if (verb != NULL && strcmp(verb, "show") == 0 && name != NULL) {
        int16_t const id = param_find_by_name(name);
        if (id < 0) {
            shell_print("no such param\r\n");
            return;
        }
        param_print_value((ParamId)id);
        return;
    }

    if (verb != NULL && strcmp(verb, "set") == 0 && name != NULL && valueStr != NULL) {
        int16_t const id = param_find_by_name(name);
        if (id < 0) {
            shell_print("no such param\r\n");
            return;
        }

        bool writeFailed = false;
        char reason[80];
        char const *err = param_apply((ParamId)id, valueStr, &writeFailed, reason, sizeof(reason));
        if (err != NULL) {
            shell_print(writeFailed ? "FAILED -- " : "REJECTED -- ");
            shell_print(err);
            shell_print("\r\n");
            return;
        }
        param_print_value((ParamId)id);
        if (g_paramDefs[id].flags & PARAM_FLAG_REBOOT) {
            shell_print("(takes effect after reboot)\r\n");
        }
        return;
    }

    shell_print("usage: param list | param show <name> | param set <name> <value>\r\n");
}

/* ---- `cfg`: machine-readable interface for clients (issue #69) ----
   One JSON object per line, every line has a "t" (type) field. Stable
   interface, documented in .docs/cli.md -- bump CFG_PROTO_VERSION on any
   change a client would have to know about. Clients ignore any line that
   doesn't start with '{' (the shell echoes typed input). Strings printed
   here are all compile-time names/labels without characters needing
   JSON escaping. */
#define CFG_PROTO_VERSION 1

static void json_kv_str(char const *key, char const *value) {
    shell_print(",\"");
    shell_print(key);
    shell_print("\":\"");
    shell_print(value);
    shell_print("\"");
}

static void json_kv_u32(char const *key, uint32_t value) {
    char num[16];
    snprintf(num, sizeof(num), "%lu", (unsigned long)value);
    shell_print(",\"");
    shell_print(key);
    shell_print("\":");
    shell_print(num);
}

/* A param's value as JSON: a string ("c"/"none") for ports, a number
   for everything else -- same letters the human CLI uses. */
static void json_kv_param_value(char const *key, ParamId id, uint32_t value) {
    if (g_paramDefs[id].kind == PARAM_KIND_PORT) {
        char letter[2] = {0, 0};
        if (value != PARAM_PORT_UNSET) {
            letter[0] = (char)('a' + value);
        }
        json_kv_str(key, value == PARAM_PORT_UNSET ? "none" : letter);
        return;
    }
    json_kv_u32(key, value);
}

/* "0=SBUS,1=CRSF" -> ,"choices":[[0,"SBUS"],[1,"CRSF"]] */
static void json_kv_choices(char const *choices) {
    shell_print(",\"choices\":[");
    char const *p = choices;
    bool first = true;
    while (*p != '\0') {
        char const *eq = strchr(p, '=');
        if (eq == NULL) {
            break;
        }
        char const *comma = strchr(eq, ',');
        size_t const labelLen = comma != NULL ? (size_t)(comma - eq - 1) : strlen(eq + 1);
        char item[40];
        snprintf(item, sizeof(item), "%s[%.*s,\"%.*s\"]", first ? "" : ",", (int)(eq - p), p, (int)labelLen,
                 eq + 1);
        shell_print(item);
        first = false;
        if (comma == NULL) {
            break;
        }
        p = comma + 1;
    }
    shell_print("]");
}

static char const *const kKindNames[] = {"number", "bool", "enum", "port"};

static void cfg_print_param(ParamId id) {
    ParamDef const *def = &g_paramDefs[id];
    uint32_t value = 0;
    param_get_u32(id, &value);

    shell_print("{\"t\":\"param\"");
    json_kv_str("name", def->name);
    json_kv_str("kind", kKindNames[def->kind]);
    json_kv_param_value("value", id, value);
    json_kv_param_value("default", id, def->defaultValue);
    if (def->kind == PARAM_KIND_NUMBER) {
        json_kv_u32("min", def->min);
        json_kv_u32("max", def->max);
    } else if (def->kind == PARAM_KIND_ENUM) {
        json_kv_choices(def->choices);
    } else if (def->kind == PARAM_KIND_PORT) {
        json_kv_str("transport", param_port_field_transport(id) == PARAM_PORT_TRANSPORT_I2C ? "i2c" : "uart");
    }
    if (def->unit == PARAM_UNIT_US) {
        json_kv_str("unit", "us");
    }
    json_kv_str("apply", (def->flags & PARAM_FLAG_REBOOT) ? "reboot" : "live");
    if (def->flags & PARAM_FLAG_HIDDEN) {
        shell_print(",\"hidden\":true");
    }
    shell_print("}\r\n");
}

static void cfg_print_ports(char const *key, ParamPortTransport transport) {
    char letters[HELM_PORT_LETTER_COUNT + 1];
    size_t n = 0;
    for (uint8_t i = 0; i < HELM_PORT_LETTER_COUNT; i++) {
        if (param_port_exists(i, transport)) {
            letters[n++] = (char)('a' + i);
        }
    }
    letters[n] = '\0';
    json_kv_str(key, letters);
}

static void cfg_info(void) {
    shell_print("{\"t\":\"info\"");
    json_kv_u32("proto", CFG_PROTO_VERSION);
    json_kv_str("board", HELM_BOARD_NAME);
    json_kv_str("fw", HELM_FW_REV);
    json_kv_u32("servos", HELM_SERVO_COUNT);
    cfg_print_ports("uart", PARAM_PORT_TRANSPORT_UART);
    cfg_print_ports("i2c", PARAM_PORT_TRANSPORT_I2C);
    shell_print(",\"slots\":[");
    for (uint8_t i = 0; i < HELM_SERVO_COUNT; i++) {
        OutputSlotInfo info;
        if (!output_slot_info(i, &info)) {
            break;
        }
        char item[48];
        if (info.isPassthrough) {
            snprintf(item, sizeof(item), "%s{\"i\":%u,\"label\":\"%s\",\"src\":\"ch%u\"}", i == 0 ? "" : ",",
                     (unsigned)i, info.label, (unsigned)(info.channelIndex + 1U));
        } else {
            snprintf(item, sizeof(item), "%s{\"i\":%u,\"label\":\"%s\",\"src\":\"%s\"}", i == 0 ? "" : ",",
                     (unsigned)i, info.label, info.function);
        }
        shell_print(item);
    }
    shell_print("]}\r\n");
}

static void cfg_error(char const *msg) {
    shell_print("{\"t\":\"error\"");
    json_kv_str("error", msg);
    shell_print("}\r\n");
}

static void cmd_cfg(const char *args) {
    if (strcmp(args, "info") == 0) {
        cfg_info();
        return;
    }
    if (strcmp(args, "params") == 0) {
        for (uint16_t i = 0; i < PARAM_COUNT; i++) {
            cfg_print_param((ParamId)i);
        }
        shell_print("{\"t\":\"end\"");
        json_kv_u32("n", PARAM_COUNT);
        shell_print("}\r\n");
        return;
    }

    char buf[SHELL_MAX_LINE_LEN];
    strncpy(buf, args, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *verb = strtok(buf, " ");
    char *name = strtok(NULL, " ");
    char *valueStr = strtok(NULL, " ");

    if (verb != NULL && strcmp(verb, "set") == 0 && name != NULL && valueStr != NULL) {
        int16_t const id = param_find_by_name(name);
        shell_print("{\"t\":\"set\"");
        json_kv_str("name", name);
        if (id < 0) {
            shell_print(",\"ok\":false");
            json_kv_str("error", "no such param");
            shell_print("}\r\n");
            return;
        }
        bool writeFailed = false;
        char reason[80];
        char const *err = param_apply((ParamId)id, valueStr, &writeFailed, reason, sizeof(reason));
        uint32_t value = 0;
        param_get_u32((ParamId)id, &value);
        shell_print(err == NULL ? ",\"ok\":true" : ",\"ok\":false");
        if (err != NULL) {
            json_kv_str("error", err);
        }
        /* Always report the value now stored -- unchanged on failure. */
        json_kv_param_value("value", (ParamId)id, value);
        shell_print("}\r\n");
        return;
    }

    cfg_error("usage: cfg info | cfg params | cfg set <name> <value>");
}
#endif

/* `reboot` command handler (#69): plain system reset. Every param write
   is already persisted synchronously by the time `param set`/`cfg set`
   replies, so there's nothing to flush except this reply itself. */
static void cmd_reboot(const char *args) {
    (void)args;
    shell_print("rebooting...\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    NVIC_SystemReset();
}

/* `diag` command handler: forwards to lib/diag's own dispatch --
   `pipeline`/`wedge` and any future bench-only diagnostic live there,
   not here, so this project's small set of real operational commands
   (status/dfu/help) stays easy to tell apart from bring-up/bench-only
   ones. See lib/diag/diag.h and .docs/cli.md. */
static void cmd_diag(const char *args) {
    diag_dispatch(args);
}

/* Task entry point for the CLI console. Brings up the USB CDC transport,
   initializes lib/shell over it, registers this project's own commands
   (`help` is registered automatically by shell_init()), then hands off to
   the shell's byte-processing loop. Never returns. */
static void cli_task(void *arg) {
    (void)arg;

    /* Bring up the transport before the shell that reads/writes over it. */
    usb_cdc_init();

    shell_init(usb_cdc_read, usb_cdc_write);

    /* shell_register()'s bool return going unchecked is exactly how a
       command table silently overflowed once before on this project's
       sibling aoa-boat-controller (see that project's docs/decisions.md)
       -- mirror its fix rather than repeating the same silent-drop risk. */
    if (!shell_register("status", "show board name and uptime", cmd_status)) {
        shell_print("WARNING: command table full, \"status\" NOT registered\r\n");
    }
#if HELM_HAS_ROM_BOOTLOADER_JUMP
    if (!shell_register("dfu", "reboot into the ROM bootloader", cmd_dfu)) {
        shell_print("WARNING: command table full, \"dfu\" NOT registered\r\n");
    }
#endif
    if (!shell_register("diag", "bench-only diagnostics -- see .docs/cli.md (try: diag pipeline / diag wedge)",
                         cmd_diag)) {
        shell_print("WARNING: command table full, \"diag\" NOT registered\r\n");
    }
#if HELM_FEATURE_PARAMS_PERSIST
    if (!shell_register("param", "persisted params -- param list | show <name> | set <name> <value>", cmd_param)) {
        shell_print("WARNING: command table full, \"param\" NOT registered\r\n");
    }
    if (!shell_register("cfg", "machine-readable (JSON lines) for the configurator -- see .docs/cli.md", cmd_cfg)) {
        shell_print("WARNING: command table full, \"cfg\" NOT registered\r\n");
    }
#endif
    if (!shell_register("reboot", "reset the board (applies reboot-only params)", cmd_reboot)) {
        shell_print("WARNING: command table full, \"reboot\" NOT registered\r\n");
    }

    shell_task(NULL);
}

void cli_start(void) {
    xTaskCreate(cli_task, "cli", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
}

#endif /* HELM_FEATURE_CLI */
