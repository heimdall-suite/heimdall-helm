# heimdall-helm

Flight-controller-class firmware for boats, part of the
[Heimdall Suite](../heimdall-kickoff.md).

Brings the flight-controller approach — pluggable sensor drivers, a
parameter system, data logging, and an active control loop driving
actuators — to boats, a category that doesn't really exist on the generic
RC TX/RX market today, where sensor/telemetry ecosystems are locked per
radio vendor (FrSky S.Port, Futaba SBUS2/FBUS, HoTT, Jeti EX).

Originally named `heimdall-nexus`; renamed to avoid colliding with the
RadioMaster Nexus-XR (a real product, unrelated to this project — a
flybarless-helicopter flight-controller board: STM32F722 + ICM42688P +
onboard dual-SX1281 ExpressLRS receiver, not a transmitter). Nexus-XR is
the preferred target hardware for this firmware to eventually run on, but
multiple board targets stay in scope.

## Four core features

1. **Sensor abstraction** — any sensor (I2C/UART/etc.), not tied to one
   radio vendor's proprietary telemetry protocol
2. **Data logging** — blackbox-style, for post-sail tuning and debugging
3. **Parameter system** — runtime-adjustable, persisted
4. **Control loop** — actively drives control surfaces (rudder, throttle,
   trim, thrusters, etc.)

## Hardware

Bench hardware on hand, with existing custom firmware to harvest working
parts from (not a straight port — the goal is a clean, robust rebuild
released as open source):

- Afroflight32 Acro Rev6
- Matek H743-WLITE

See [.docs/hardware.md](.docs/hardware.md) for details.

## Docs

- [.docs/hardware.md](.docs/hardware.md) — target boards, open hardware
  questions
- [.agents/AGENTS.md](.agents/AGENTS.md) / [CLAUDE.md](CLAUDE.md) —
  instructions for AI coding agents working in this repo

## Status

Built from scratch, not forked from `aoa-boat-controller` (an existing,
narrower AoA/trim-tab-only project on the same two boards) — that project's
"things just run in the loop" growing pains are exactly what this repo's
architecture is meant to avoid, designed in up front instead of retrofitted.

Toolchain decided: **PlatformIO, `framework = stm32cube`** (raw HAL/LL, no
Arduino) **+ FreeRTOS** (vendored under [vendor/freertos-kernel](vendor/freertos-kernel),
preemptive scheduling for real fault isolation between modules — see that
folder's README for why it's vendored rather than a submodule or registry
package). First target: Matek H743-WLITE (most headroom of the two bench
boards). RadioMaster Nexus-XR stays the preferred long-term target once
acquired.

Current milestone: `[env:matek_h743]` builds and links cleanly (HAL +
FreeRTOS, one heartbeat task, scheduler started) — proves the toolchain
end to end. Not yet flashed or bench-verified on real hardware (no
ST-Link/CubeProgrammer tooling in the environment this was built in). No
clock config, drivers, or the module/scheduler architecture itself yet —
this is a bring-up-only milestone, not a feature.
