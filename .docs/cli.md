# CLI

Status: implemented and bench-verified on both `matek_h743` (issue #1)
and `afroflight32` (#12/#13) — see "Which boards have it" below. Not
ported to `nexus_xr` at all.

A command console the host reaches over what looks like a USB virtual COM
port, so the board can be talked to from a normal serial terminal without
any extra tooling on the host. "Looks like" is deliberate: on
`matek_h743` that's genuinely this chip's own native USB CDC-ACM device;
on `afroflight32` it's a plain UART (USART1) bridged through an onboard
USB-serial converter chip that does the actual USB side in external
hardware, confirmed against aoa-boat-controller's real firmware for that
exact physical board (see `lib/usb_cdc/stm32f1.c`'s own header comment).
The host-visible result is the same either way; how it gets there isn't
-- that's exactly what `lib/usb_cdc/`'s interface exists to hide from
everything above it.

## How it's wired

Five pieces, split the way [lib/README.md](../lib/README.md) describes
(chip-not-board for hardware, hardware-independent logic gets its own
folder too):

| Layer | Where | What it knows about |
|---|---|---|
| Transport | `lib/usb_cdc/` | Whatever byte-stream reaches the host as a COM port, one implementation per chip: `stm32h7.c` (native USB CDC-ACM) and `stm32f1.c` (a plain UART, see this page's intro) |
| Shell engine | `lib/shell/` | Nothing hardware — line editing, command dispatch, given bytes in/out as function pointers |
| This project's CLI | `lib/cli/` | Wires `shell` to `usb_cdc`, registers this project's own commands |
| ROM bootloader reboot | `lib/bootloader/` | Software jump into the chip's ROM bootloader, one implementation per chip (`stm32h7.c`: USB DFU; `stm32f1.c`: plain UART/AN3155, issue #28) |
| Bench diagnostics | `lib/diag/` | Bring-up/bench-only subcommands (`pipeline`, `wedge`), all dispatched through the CLI's single `diag` command |

`src/main.c` starts it with a single `cli_start()` call, gated on
`board_features.h`'s `HELM_FEATURE_CLI`.

## Which boards have it

Gated by three INDEPENDENT flags in each board's `board_features.h`, each
requiring its own bench-confirmed, from-real-source implementation before
it's turned on (see `lib/bootloader/stm32h7.c`'s header comment for the
standard) — not just "not built yet". Independent matters here: a board
can have a CLI without the other two (`lib/cli/cli.c` and `src/main.c`
both gate their `HELM_HAS_ROM_BOOTLOADER_JUMP`-dependent calls on that flag
specifically, not on `HELM_FEATURE_CLI` — issue #13 fixed both call sites
after finding they'd otherwise fail to link on exactly this board).

| Flag | Means |
|---|---|
| `HELM_FEATURE_CLI` | Build `lib/shell` + `lib/cli` + `lib/usb_cdc` and start the console task |
| `HELM_HAS_ROM_BOOTLOADER_JUMP` | This chip has a bench-confirmed software jump into its ROM bootloader, so the CLI's `dfu` command works -- not necessarily USB DFU class specifically, see the flag's own comment in `board_features.h` |
| `HELM_HAS_DEBUG_LED` | `board_led_toggle()` exists and its pin/polarity is bench-confirmed, so `lib/debug/heartbeat.c` has something to blink |

| Board | `HELM_FEATURE_CLI` | `HELM_HAS_ROM_BOOTLOADER_JUMP` | `HELM_HAS_DEBUG_LED` |
|---|---|---|---|
| `matek_h743` | 1 (bench-verified) | 1 (USB DFU, `lib/bootloader/stm32h7.c`) | 1 |
| `afroflight32` | 1 (bench-verified) | 1 (plain UART ROM bootloader, AN3155 protocol, `lib/bootloader/stm32f1.c`, issue #28) | 1 (bench-verified, PB4 "CAL" LED) |
| `nexus_xr` | 0 | 0 | 0 |

Getting `afroflight32`'s CLI actually working on the bench needed two
fixes beyond the transport/CLI wiring itself, both found during #13's
bring-up and both still in place as permanent fixes, not workarounds:

- `src/main.c` sets `SCB->VTOR = FLASH_BASE` for STM32F1 boards. This
  board's bootloader entry (BOOT0 strapped high, then a software "Go"
  jump, not a real reset) leaves interrupt vectoring pointed at the ROM
  bootloader's own stale vector table -- code runs fine until the first
  thing that depends on a real interrupt (SysTick, and therefore every
  `HAL_Delay()`/task delay), which then hangs forever. Only a genuine
  reset re-samples BOOT0 and fixes this on its own.
- `boards/afroflight32/FreeRTOSConfig.h`'s `configTOTAL_HEAP_SIZE` went
  from 6KB to 10KB -- 6KB was an unmeasured guess that turned out too
  small once `vTaskStartScheduler()`'s own IDLE/timer-service task
  creation is counted alongside this board's 8 application tasks +
  queues. Bisected via LED checkpoint markers (no debug probe available)
  before landing on this as the actual cause.

## Connecting

**`matek_h743`**: enumerates as a native CDC virtual COM port, VID:PID
`0483:5740` (deliberately different from the ROM bootloader's
`0483:df11`, so the host can tell "running app, CLI available" and
"sitting in DFU" apart on sight — see `boards/matek_h743/usbd_desc.c`).
Baud rate doesn't matter for a USB CDC device.

**`afroflight32`**: the "USB" port is an onboard USB-serial converter
chip, so the host sees THAT chip's own VID:PID (whatever silicon it is),
not anything this firmware controls — no descriptors to look at
here. `lib/usb_cdc/stm32f1.c` talks to it over USART1 at 115200 8N1;
the host's serial terminal needs to match that baud rate, unlike the
native-USB case above.

Open either in any serial terminal once connected.

## Existing commands

| Command | Does |
|---|---|
| `help` | Lists every registered command and its help text (built in, registered by `shell_init()`) |
| `status` | Prints the board name and uptime |
| `dfu` | Only on boards with `HELM_HAS_ROM_BOOTLOADER_JUMP` set (`matek_h743`, `afroflight32`) — reboots into the chip's ROM bootloader (USB DFU on matek_h743, plain UART/AN3155 on afroflight32), ready for `pio run -t upload` with no manual BOOT0-strap needed |
| `diag pipeline` | Shows the RX→Mapping→Control→Output→Servo chain's final stage output (status + channel values) |
| `diag wedge` | Bench-only: spins a task above the supervisor's priority to prove IWDG actually resets the board (#5) — the board reboots ~250ms after running this |
| `diag telemetry` | Dumps the telemetry table's current state (issue #16) — every field, value/status/age, even before any protocol adapter (#18/#19) exists |
| `diag sport` | Only on boards with `HELM_HAS_SPORT_UART` set (`matek_h743`) — poll-marker vs. poll-match counters (issue #18), for telling "receiver isn't polling" apart from "polling, ID never matches" apart from "genuinely working" |
| `diag imu` | Only on boards with `HELM_HAS_IMU` set (`matek_h743`, `afroflight32`) — dumps the IMU sample queue's status + values, real chip reads as of #26/#27 |
| `diag baro` | Only on boards with `HELM_HAS_BARO` set (`matek_h743`, `afroflight32`) — dumps the baro sample queue's status + pressure/temperature, real chip reads as of #23 |
| `param list` / `param show <name>` / `param set <name> <value>` | Only on boards with `HELM_FEATURE_PARAMS_PERSIST` set (`matek_h743`, `afroflight32` — see `lib/params/`, issue #32) — generic get/set/enumerate over the persisted-param store. Two params so far: `test_counter` (throwaway proof, issue #32) and `input_mode` (real: `lib/rx/rx.c`'s runtime SBUS/CRSF pick, `0`/`1`, issue #10 — takes effect on next boot, `rx_start()` only reads it once at container-start). Remaining consumers (function mapping, servo endpoints, PID gains) are future work on top of this same interface |

## Adding a command

**A real operational command** (something a normal flight-line workflow
needs, like `status`/`dfu`) lives in `lib/cli/cli.c`, following
`cmd_status`/`cmd_dfu`'s shape:

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

**A bring-up/bench-only diagnostic** (inspection like `pipeline`, or
hazardous like `wedge`) does NOT get its own top-level `shell_register()`
call. Add a subcommand to `lib/diag/diag.c`'s `diag_dispatch()` instead —
a `static void diag_mything(void)` plus one more `strcmp` branch — and
list it in this file's command table above. This keeps the 16-slot table
for commands a real workflow needs, and keeps every hazardous/inspection-
only action grep-able in one file rather than scattered across the
top-level namespace.
