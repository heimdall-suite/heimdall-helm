# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Cross-tool agent instructions (also valid for Claude Code) live in
[.agents/AGENTS.md](.agents/AGENTS.md) — read that first. This file only
adds Claude-Code-specific notes on top.

## Status

Toolchain decided, and bring-up + multi-target structure milestone
reached: PlatformIO, `framework = stm32cube` (raw HAL/LL, no Arduino) +
FreeRTOS. Three targets from the start: `matek_h743` and `afroflight32`
both build and link cleanly with real, bench-derived clock configs;
`nexus_xr` is structurally present but intentionally `#error`s (no
confirmed hardware data — see `boards/nexus_xr/board.h`). Both `matek_h743` and `afroflight32` are
flashed/bench-verified on real hardware, neither via ST-Link/
CubeProgrammer: `matek_h743` over its native USB DFU bootloader (`pio run
-e matek_h743 -t upload`); `afroflight32` does NOT flash via USB DFU (an
earlier, now-corrected guess) — its "USB" port is an onboard USB-serial
converter chip wired to a plain UART, not this chip's native USB
peripheral, so it flashes over that same UART against the STM32's
built-in serial bootloader instead. Both boards' CLIs now have a software
`dfu` command that jumps straight into their respective ROM bootloader
(issue #28 added afroflight32's, over the same UART/AN3155 protocol,
alongside matek_h743's existing USB-DFU one) — no manual BOOT0-strap
needed for either board anymore. See `.agents/AGENTS.md` and
`.docs/cli.md` for the full story. The Input→Mapping→Control→Output→Servo
chain (issue #7's original bring-up stub) now has real logic end to end
on both boards, not just plumbing: real SBUS decode on matek_h743 with
runtime SBUS/CRSF selection via a persisted param (issue #10); a
first-pass, hardcoded function/input mapping (issue #34) and a
placeholder no-op control loop (issue #35) feeding real output mapping
with per-slot failsafe/reverse/endpoint trim (issues #36/#37);
driving a real PWM servo driver with a persisted, per-boat frame-rate
choice (50/250/333Hz), bench-verified against real hardware with an
external USB logic analyzer, not just CLI-reported values (issue #31).
Real per-axis control-loop math (PID/attitude hold) and a genuinely
configurable, params-backed function/output mapping table (today's
tables are hardcoded, proving the mechanism, not final per-boat config)
are still future work. Confirm further architecture direction with the
user before building those out.

Build: `pio run -e <matek_h743|afroflight32|nexus_xr>` (from repo root;
`nexus_xr` fails intentionally). No lint/test commands yet.

## Layout

- `README.md` — short overview + status, points into the docs below
- `.docs/hardware.md` — target board details
- `.docs/cli.md` — the USB-serial CLI: wiring, which boards have it, how
  to add a command
- `.agents/AGENTS.md` — full cross-tool architecture/context notes (the
  primary reference — this file doesn't repeat it)
- `platformio.ini` — one `[env:...]` per board target; see its own
  comments for the multi-target wiring approach and FPU-flag wrinkles
  found during bring-up (don't duplicate FPU flags between `build_flags`
  and `custom_helm_fpu_flags` — see `scripts/add_freertos.py`'s comment
  for why that already caused a real bug once)
- `boards/<target>/` — per-board `board.c`/`board.h` (clock config,
  `board_init()`), `board_features.h` (capability flags), `FreeRTOSConfig.h`
- `lib/` — chip/protocol-based drivers and hardware-independent logic,
  shared across boards (currently empty — see `lib/README.md` for the
  convention before adding anything here)
- `vendor/freertos-kernel/` — vendored FreeRTOS-Kernel subset, Cortex-M3
  and Cortex-M7 ports both in use (see its README for why it's not a
  submodule)
- `scripts/add_board.py`, `scripts/add_freertos.py` — PlatformIO
  extra_scripts wiring the per-board source and vendored kernel into each
  env's build; both read `custom_helm_*` options from `platformio.ini`
- `src/main.c` — single composition root for every board; currently just
  the bring-up stub

## Suite context

This is one repo in the larger Heimdall Suite (personal RC electronics
project, org `github.com/heimdall-suite`). Full suite-level context —
including `heimdall-module` and `heimdall-switch`, which this firmware may
talk to — lives in `../heimdall-kickoff.md` (one level up, outside this
repo).
