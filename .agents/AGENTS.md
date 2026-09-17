# Agent instructions

Cross-tool instructions for any AI coding agent working in this repo (Claude
Code has its own additional notes in `/CLAUDE.md` at the repo root).

## Status

Pre-firmware: structure and docs only, no source code, no build system. The
toolchain/architecture has not been chosen — do not assume one and do not
scaffold a build system without confirming the choice first. There are no
build, lint, or test commands yet.

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
RadioMaster Nexus XR transmitter (a real product, unrelated to this
project). RadioMaster Nexus XR is the preferred target transmitter, but
don't hard-lock design decisions to it — multiple transmitter/receiver
targets stay in scope.

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

Confirm the toolchain/architecture choice with the user before generating
any build files or firmware source layout under `src/` — whether to build
from scratch, harvest specific modules from the bench firmware, or base on
an existing open-source stack (ArduPilot Rover, PX4) is a decision with
real architectural consequences that hasn't been made yet.
