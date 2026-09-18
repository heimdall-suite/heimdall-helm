#include "board_features.h"

#if HELM_FEATURE_CLI

#include "cli.h"

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "bootloader.h"
#include "shell.h"
#include "usb_cdc.h"
#include <stdio.h>

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

/* `dfu` command handler: reboots into the ROM USB DFU bootloader from the
   same USB cable the CLI itself runs over -- the reason this feature was
   built in the first place, closing the loop with lib/bootloader so no
   BOOT-button/ST-Link is needed to reflash. */
static void cmd_dfu(const char *args) {
    (void)args;
    shell_print("rebooting into ROM DFU bootloader...\r\n");
    bootloader_request_dfu();
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
    if (!shell_register("dfu", "reboot into the ROM USB DFU bootloader", cmd_dfu)) {
        shell_print("WARNING: command table full, \"dfu\" NOT registered\r\n");
    }

    shell_task(NULL);
}

void cli_start(void) {
    xTaskCreate(cli_task, "cli", configMINIMAL_STACK_SIZE * 4, NULL, 1, NULL);
}

#endif /* HELM_FEATURE_CLI */
