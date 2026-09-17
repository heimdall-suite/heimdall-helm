# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Cross-tool agent instructions (also valid for Claude Code) live in
[.agents/AGENTS.md](.agents/AGENTS.md) — read that first. This file only
adds Claude-Code-specific notes on top.

## Status

Toolchain decided and bring-up milestone reached: PlatformIO,
`framework = stm32cube` (raw HAL/LL, no Arduino) + FreeRTOS, targeting the
Matek H743-WLITE first (`[env:matek_h743]`). `pio run` builds and links
cleanly. Not yet flashed/bench-verified on real hardware. The actual
module/scheduler architecture (extensibility, fault isolation/HA) is still
undesigned — the current `src/main.c` is a bring-up stub (one heartbeat
task), not the real firmware structure. Confirm architecture direction with
the user before building it out.

Build: `pio run` (from repo root). No lint/test commands yet.

## Layout

- `README.md` — short overview + status, points into the docs below
- `.docs/hardware.md` — target board details
- `.agents/AGENTS.md` — full cross-tool architecture/context notes (the
  primary reference — this file doesn't repeat it)
- `platformio.ini` — build config; see its own comments for the FPU/
  FreeRTOS-link wrinkles found during bring-up
- `vendor/freertos-kernel/` — vendored FreeRTOS-Kernel subset (see its
  README for why it's not a submodule)
- `config/FreeRTOSConfig.h` — FreeRTOS config for this target
- `scripts/add_freertos.py` — PlatformIO extra_script wiring the vendored
  kernel into the build
- `src/` — firmware source; currently just the bring-up stub

## Suite context

This is one repo in the larger Heimdall Suite (personal RC electronics
project, org `github.com/heimdall-suite`). Full suite-level context —
including `heimdall-module` and `heimdall-switch`, which this firmware may
talk to — lives in `../heimdall-kickoff.md` (one level up, outside this
repo).
