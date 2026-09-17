# heimdall-helm

Flight-controller-class firmware for boats, part of the
[Heimdall Suite](../heimdall-kickoff.md).

Brings the flight-controller approach — pluggable sensor drivers, a
parameter system, data logging, and an active control loop driving
actuators — to boats, a category that doesn't really exist on the generic
RC TX/RX market today, where sensor/telemetry ecosystems are locked per
radio vendor (FrSky S.Port, Futaba SBUS2/FBUS, HoTT, Jeti EX).

Originally named `heimdall-nexus`; renamed to avoid colliding with the
RadioMaster Nexus XR transmitter (a real product, unrelated to this
project). RadioMaster Nexus XR is the preferred target transmitter, but the
firmware is not locked to it — multiple transmitter/receiver targets stay
in scope.

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

No firmware yet. Toolchain/architecture not decided — whether to build from
scratch, harvest specific modules from the existing bench firmware, or base
on an existing open-source stack (e.g. ArduPilot Rover, PX4) is still open.
`src/` is a placeholder until that's settled.
