# Agent instructions

Cross-tool instructions for any AI coding agent working in this repo (Claude
Code has its own additional notes in `/CLAUDE.md` at the repo root).

## Status

Toolchain decided and bring-up milestone reached: PlatformIO,
`framework = stm32cube` (raw HAL/LL, no Arduino — the user explicitly
doesn't want Arduino, having weighed it against the pain
`aoa-boat-controller`'s `platformio.ini` documents fighting the Arduino
core's assumptions) + FreeRTOS (vendored, see `vendor/freertos-kernel/`'s
README), targeting the Matek H743-WLITE first via `[env:matek_h743]`.
`pio run` builds and links cleanly from repo root — no lint/test commands
yet. Not flashed/bench-verified on real hardware (no ST-Link/
CubeProgrammer tooling available where this was built).

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

There's existing bench hardware with working custom firmware (targeting
Afroflight32 Acro Rev6 and Matek H743-WLITE) to harvest good parts from —
see [hardware.md](../.docs/hardware.md). The long-term goal is a clean,
robust rebuild released as open source, not a straight port of that bench
code.

## Before scaffolding

Toolchain/build-system scaffolding is done — don't re-litigate it. What's
still open and needs the user's confirmation before building further:
the module/task architecture (task boundaries, how a module registers
itself, priority/budget scheme), and the fault-isolation/HA model (what
happens when a task blows its budget, a driver call blocks, or a sensor
goes away mid-flight/mid-sail). Don't invent these unilaterally and start
writing sensor/log/param/control-loop code on top of the bring-up stub
without that conversation happening first.
