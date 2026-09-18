#ifndef HELM_SHELL_H
#define HELM_SHELL_H

#include <stdbool.h>
#include <stdint.h>

/* Minimal line-based command console. Hardware-independent (see
   lib/README.md's "hardware-independent logic lives here too" section)
   -- it knows nothing about USB/UART, just bytes in and bytes out, given
   to it as function pointers at init. lib/cli is this project's one
   consumer today, wiring it to lib/usb_cdc as transport and registering
   its own commands; a board without lib/cli built simply never calls
   shell_init()/creates shell_task, gated by board_features.h's
   HELM_FEATURE_CLI. */

typedef uint32_t (*ShellReadFn)(uint8_t *buf, uint32_t maxLen);
typedef void (*ShellWriteFn)(const uint8_t *buf, uint32_t len);

/* args points into the parsed line, at the first character after the
   command word (or "" if none) -- valid only for the duration of the
   handler call. */
typedef void (*ShellCommandHandler)(const char *args);

#define SHELL_MAX_COMMANDS 16
#define SHELL_MAX_LINE_LEN 64

/* Resets the command table and registers the built-in `help` command.
   Call once before any shell_register()/shell_task() call. */
void shell_init(ShellReadFn readFn, ShellWriteFn writeFn);

/* Registers a command. Returns false (and registers nothing) if the
   table is already full -- same bounds-checked shape as this project's
   sibling aoa-boat-controller's SerialCli::addCommand(), chosen there
   after an unchecked version once silently dropped a command; callers
   should check this, not ignore it. */
bool shell_register(const char *name, const char *help, ShellCommandHandler handler);

/* Writes a NUL-terminated string to the console. */
void shell_print(const char *str);

/* FreeRTOS task entry point -- polls the transport, echoes input, and
   dispatches complete lines. Create with xTaskCreate() after shell_init()
   and every shell_register() call. Never returns. */
void shell_task(void *arg);

#endif /* HELM_SHELL_H */
