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

        /* Issue #54 -- `.port` fields go through the validated setter
           (board-existence + collision checks, params.h's own
           param_set_port() comment), not the plain param_set_u32() every
           other param here still uses. */
        if (param_id_is_port_field((ParamId)id)) {
            uint32_t portValue = 0;
            if (!param_parse_port_value(valueStr, &portValue)) {
                shell_print("invalid port value -- expected a letter (a-i) or \"none\"\r\n");
                return;
            }

            ParamPortTransport const transport = param_port_field_transport((ParamId)id);
            if (portValue != PARAM_PORT_UNSET && !param_port_exists((uint8_t)portValue, transport)) {
                shell_print("no such port on this board\r\n");
                return;
            }
            if (!param_set_port((ParamId)id, (uint8_t)portValue, transport)) {
                shell_print("FAILED -- already claimed by another subsystem, or flash write error\r\n");
                return;
            }
            param_print_value((ParamId)id);
            return;
        }

        uint32_t const value = (uint32_t)strtoul(valueStr, NULL, 0);
        if (!param_set_u32((ParamId)id, value)) {
            shell_print("FAILED to persist -- flash write error, value NOT saved\r\n");
            return;
        }
        param_print_value((ParamId)id);
        return;
    }

    shell_print("usage: param list | param show <name> | param set <name> <value>\r\n");
}
#endif

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
#endif

    shell_task(NULL);
}

void cli_start(void) {
    xTaskCreate(cli_task, "cli", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
}

#endif /* HELM_FEATURE_CLI */
