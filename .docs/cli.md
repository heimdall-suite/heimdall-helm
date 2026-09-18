# CLI

Status: implemented and bench-verified on `matek_h743` (issue #1). Not
ported to `afroflight32`/`nexus_xr` yet — see "Which boards have it"
below.

A command console running over a USB virtual COM port (CDC-ACM), so the
board can be talked to from a normal serial terminal without any extra
tooling on the host.

## How it's wired

Four pieces, split the way [lib/README.md](../lib/README.md) describes
(chip-not-board for hardware, hardware-independent logic gets its own
folder too):

| Layer | Where | What it knows about |
|---|---|---|
| Transport | `lib/usb_cdc/` | USB CDC-ACM, one implementation per chip (currently just `stm32h7.c`) |
| Shell engine | `lib/shell/` | Nothing hardware — line editing, command dispatch, given bytes in/out as function pointers |
| This project's CLI | `lib/cli/` | Wires `shell` to `usb_cdc`, registers this project's own commands |
| DFU reboot | `lib/bootloader/` | Software jump into the chip's ROM DFU bootloader, one implementation per chip |

`src/main.c` starts it with a single `cli_start()` call, gated on
`board_features.h`'s `HELM_FEATURE_CLI`.

## Which boards have it

Gated by three flags in each board's `board_features.h`, each requiring
its own bench-confirmed, from-real-source implementation before it's
turned on (see `lib/bootloader/stm32h7.c`'s header comment for the
standard) — not just "not built yet":

| Flag | Means |
|---|---|
| `HELM_FEATURE_CLI` | Build `lib/shell` + `lib/cli` + `lib/usb_cdc` and start the console task |
| `HELM_HAS_ROM_BOOTLOADER_DFU` | This chip has a bench-confirmed ROM DFU jump, so the CLI's `dfu` command works |
| `HELM_HAS_DEBUG_LED` | `board_led_toggle()` exists and its pin/polarity is bench-confirmed, so `lib/debug/heartbeat.c` has something to blink |

Today only `matek_h743` has all three set to `1`.

## Connecting

The board enumerates as a CDC virtual COM port, VID:PID `0483:5740`
(deliberately different from the ROM bootloader's `0483:df11`, so the
host can tell "running app, CLI available" and "sitting in DFU" apart on
sight — see `boards/matek_h743/usbd_desc.c`). Open it in any serial
terminal; baud rate doesn't matter for a USB CDC device.

## Existing commands

| Command | Does |
|---|---|
| `help` | Lists every registered command and its help text (built in, registered by `shell_init()`) |
| `status` | Prints the board name and uptime |
| `dfu` | Reboots into the ROM USB DFU bootloader, ready for `pio run -t upload` |

## Adding a command

Commands live in `lib/cli/cli.c`, following `cmd_status`/`cmd_dfu`'s
shape:

```c
static void cmd_mycommand(const char *args) {
    /* args is "" if the user typed no arguments */
    shell_print("hello from mycommand\r\n");
}
```

Then register it in `cli_task()`, alongside the existing commands:

```c
if (!shell_register("mycommand", "one-line help text", cmd_mycommand)) {
    shell_print("WARNING: command table full, \"mycommand\" NOT registered\r\n");
}
```

`SHELL_MAX_COMMANDS` (`lib/shell/shell.h`) caps the table at 16 — raise it
there if a project ever needs more. Always check `shell_register()`'s
return value; a silently dropped command is exactly the bug this
bounds-checked shape was chosen to avoid (see that function's own header
comment).
