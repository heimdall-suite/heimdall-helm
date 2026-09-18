# Agent instructions

Cross-tool instructions for any AI coding agent working in this repo (Claude
Code has its own additional notes in `/CLAUDE.md` at the repo root).

## Status

Toolchain decided, and bring-up + multi-target structure milestone
reached: PlatformIO, `framework = stm32cube` (raw HAL/LL, no Arduino — the
user explicitly doesn't want Arduino, having weighed it against the pain
`aoa-boat-controller`'s `platformio.ini` documents fighting the Arduino
core's assumptions) + FreeRTOS (vendored, see `vendor/freertos-kernel/`'s
README).

Three targets are structurally supported from the start (`platformio.ini`
has one `[env:...]` per board): `matek_h743` (STM32H743) and `afroflight32`
(STM32F103) both build and link cleanly, each with its own real,
bench-derived clock config ported from `aoa-boat-controller`. `nexus_xr`
(STM32F722, the RadioMaster Nexus-XR) is structurally present —
`boards/nexus_xr/` exists, `platformio.ini` has the env — but
`boards/nexus_xr/board.c` deliberately `#error`s: no confirmed HSE crystal
value or bench-verified pin map exists for that board yet, and fabricating
one would be worse than not having it. Don't remove that `#error` without
real hardware or a real schematic to work from.

The per-board split follows `lib/README.md`'s chip-not-board axis: drivers
go in `lib/<subsystem>/<chip>.c`, reused across any board with that chip,
NOT duplicated per board the way `aoa-boat-controller` did (its own
`platformio.ini` documents the LDF trap that split caused — nested
per-target library folders silently drop out of the build unless
explicitly listed in `lib_extra_dirs`). `boards/<target>/board.c` only
decides which driver instances a board wires up.

`pio run -e <target>` builds each env individually from repo root (no
lint/test commands yet). `matek_h743` is now flashed/bench-verified on
real hardware: `pio run -e matek_h743 -t upload` goes out over its ROM
USB DFU bootloader (dfu-util) — no ST-Link/CubeProgrammer involved, and
none is used for any target in this project. Once firmware with
`HELM_FEATURE_CLI`/`HELM_HAS_ROM_BOOTLOADER_DFU` is already running, its
own `dfu` CLI command (`lib/bootloader/stm32h7.c`) jumps it into that
bootloader in software, over the same USB cable, no button press needed.
`afroflight32` is planned to flash the same USB-DFU way and will get a
CLI eventually, but won't get the `dfu` bootloader-jump command even
then — `HELM_HAS_ROM_BOOTLOADER_DFU` is a separate capability from
`HELM_FEATURE_CLI`, not implied by it (see `boards/afroflight32/
board_features.h`). Its bootloader has to be entered manually instead,
and it hasn't been bench-verified yet. `nexus_xr` remains unbuildable on
purpose (see above).

The real module/scheduler architecture (extensibility, fault isolation/HA
— what can and can't fail) is NOT designed yet. `src/main.c` is a bring-up
stub only (one heartbeat task, scheduler started) proving the toolchain
works end to end — do not treat it as the real firmware structure or build
features directly onto it without first confirming the actual architecture
with the user.

## Context

Part of the Heimdall Suite (personal RC electronics project, org
`github.com/heimdall-suite`). Suite-level context — including
`heimdall-module` and `heimdall-switch` — lives in `../heimdall-kickoff.md`,
one level up in the local workspace (not tracked in this repo). Read that
before assuming intent not captured here.

## What this firmware does

`heimdall-helm` brings the flight-controller approach — pluggable sensor
drivers, a parameter system, data logging, and an active control loop
driving actuators — to boats. This doesn't really exist on the generic RC
TX/RX market today: sensor/telemetry ecosystems are locked per radio vendor
(FrSky S.Port, Futaba SBUS2/FBUS, HoTT, Jeti EX), unlike the vendor-agnostic
sensor bus approach flight controllers (ArduPilot/PX4/Betaflight) take.

Originally named `heimdall-nexus`; renamed to avoid colliding with the
RadioMaster Nexus-XR (a real product, unrelated to this project — a
flybarless-helicopter flight-controller board: STM32F722 + ICM42688P +
onboard dual-SX1281 ExpressLRS receiver, not a transmitter). Nexus-XR is
the preferred target hardware for this firmware to eventually run on, but
don't hard-lock design decisions to it — multiple board targets stay in
scope.

Four core features to keep central to any architecture decision:

1. **Sensor abstraction** — any sensor (I2C/UART/etc.), not tied to one
   radio vendor's proprietary telemetry protocol
2. **Data logging** — blackbox-style, for post-sail tuning and debugging
3. **Parameter system** — runtime-adjustable, persisted
4. **Control loop** — actively drives control surfaces (rudder, throttle,
   trim, thrusters, etc.)

Built from scratch, not a straight port of `aoa-boat-controller` — but
specific known-good facts from it (bench-confirmed clock configs, chip
identities) are worth pulling in deliberately, same way `boards/matek_h743/
board.c` and `boards/afroflight32/board.c` already did for their clock
trees. See [hardware.md](../.docs/hardware.md) for target board details.

`afroflight32` is deliberately resource-constrained (20KB RAM / 128KB
flash — the H743's FreeRTOS heap alone is configured larger than this
board's entire RAM) and carries a genuinely reduced feature set, not just
"not implemented yet." Each board's `board_features.h` declares
`HELM_HAS_*` (hardware facts: is a chip physically wired) and
`HELM_FEATURE_*` (deliberate software capability toggles) — check
`boards/afroflight32/board_features.h`'s `HELM_FEATURE_*` values before
assuming a feature belongs on every target equally; some of those values
are first-pass placeholders marked TODO, not final decisions, so confirm
with the user before treating them as settled.

## Before scaffolding

Toolchain/build-system/multi-target scaffolding is done — don't
re-litigate it, and don't add a fourth board without following the
existing `boards/<target>/` pattern. What's still open and needs the
user's confirmation before building further: the module/task architecture
(task boundaries, how a module registers itself, priority/budget scheme),
and the fault-isolation/HA model (what happens when a task blows its
budget, a driver call blocks, or a sensor goes away mid-flight/mid-sail).
Don't invent these unilaterally and start writing sensor/log/param/
control-loop code on top of the bring-up stub without that conversation
happening first.
