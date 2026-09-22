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

## Hardware — three targets, supported from the start

| Target (`platformio.ini` env) | MCU | Status |
|---|---|---|
| `matek_h743` | STM32H743 (Cortex-M7, dual-precision FPU) | [![bench-verified #7](https://img.shields.io/badge/bench--verified-%237-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/7) bring-up chain <br />[![bench-verified #31](https://img.shields.io/badge/bench--verified-%2331-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/31) servo/output <br />flashed over USB DFU |
| `afroflight32` | STM32F103 (Cortex-M3, no FPU) | [![bench-verified #13](https://img.shields.io/badge/bench--verified-%2313-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/13) bring-up chain <br />[![bench-verified #28](https://img.shields.io/badge/bench--verified-%2328-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/28) DFU <br />flashed over its UART ROM bootloader, deliberately reduced feature set (20KB RAM) |
| `nexus_xr` | STM32F722 (Cortex-M7, single-precision FPU) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) <br />`board.c` intentionally `#error`s, no confirmed pin/clock data exists yet |

See [boards/](boards/) for the per-target wiring/clock config/feature
flags themselves.

## Features

Which subsystem is real (a bench-confirmed driver, not the
fixed-test-data/stub fallback) on which target — same data as
[.docs/hardware.md](.docs/hardware.md)'s feature matrix, kept here too
for a quick glance without leaving this page. Badge color: green =
closed issue, bench-verified on real hardware; yellow = code merged to
`main` but the issue stays open pending bench-verification (this repo's
convention, see Status below); blue = open issue, no code yet; grey =
not applicable / blocked on hardware.

| Feature | `afroflight32` | `matek_h743` | `nexus_xr` |
|---|---|---|---|
| Control: attitude estimation (pitch + roll) | [![planned #49](https://img.shields.io/badge/planned-%2349-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/49) | [![planned #49](https://img.shields.io/badge/planned-%2349-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/49) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Pitch mode plumbing (placeholder passthrough) | [![done #35](https://img.shields.io/badge/done-%2335-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/35)&nbsp;[![pending verify #38](https://img.shields.io/badge/pending_verify-%2338-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/38) | [![done #35](https://img.shields.io/badge/done-%2335-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/35)&nbsp;[![pending verify #38](https://img.shields.io/badge/pending_verify-%2338-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/38) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Pitch real PID/attitude-hold | [![planned #50](https://img.shields.io/badge/planned-%2350-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/50) | [![planned #50](https://img.shields.io/badge/planned-%2350-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/50) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Roll hold (new axis, architecture TBD) | [![planned #51](https://img.shields.io/badge/planned-%2351-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/51) | [![planned #51](https://img.shields.io/badge/planned-%2351-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/51) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: CLI | [![done #13](https://img.shields.io/badge/done-%2313-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/13) | [![done #1](https://img.shields.io/badge/done-%231-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/1) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: IWDG crash safety | [![done #11](https://img.shields.io/badge/done-%2311-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/11) | [![done #5](https://img.shields.io/badge/done-%235-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/5) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: params persistence (generic store) | [![done #32](https://img.shields.io/badge/done-%2332-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/32) | [![done #32](https://img.shields.io/badge/done-%2332-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/32) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: ROM bootloader `dfu` jump | [![done #28](https://img.shields.io/badge/done-%2328-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/28) <br />(UART/AN3155) | [![done #1](https://img.shields.io/badge/done-%231-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/1)/[![#3](https://img.shields.io/badge/done-%233-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/3) <br />(USB DFU) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Logging: blackbox | [![planned #45](https://img.shields.io/badge/planned-%2345-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/45) <br />(SPI NOR, storage backend not started) | [![planned #44](https://img.shields.io/badge/planned-%2344-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/44) <br />(SD card, storage backend not started) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Mapping: function/input | [![done #34](https://img.shields.io/badge/done-%2334-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/34) | [![done #34](https://img.shields.io/badge/done-%2334-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/34) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Output: mapping (failsafe/reverse/endpoint) | [![done #36](https://img.shields.io/badge/done-%2336-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/36)&nbsp;[![done #37](https://img.shields.io/badge/done-%2337-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/37) | [![done #36](https://img.shields.io/badge/done-%2336-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/36)&nbsp;[![done #37](https://img.shields.io/badge/done-%2337-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/37) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Output: params-backed trim | [![pending verify #39](https://img.shields.io/badge/pending_verify-%2339-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/39) | [![pending verify #39](https://img.shields.io/badge/pending_verify-%2339-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/39) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Peripheral: GPS — NMEA 0183 decode | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) | [![pending verify #40](https://img.shields.io/badge/pending_verify-%2340-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/40) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Peripheral: magnetometer | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) | [![planned #48](https://img.shields.io/badge/planned-%2348-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/48) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| RX: CRSF decode | [![planned #9](https://img.shields.io/badge/planned-%239-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/9) <br />(stub) | [![planned #9](https://img.shields.io/badge/planned-%239-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/9) <br />(stub) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| RX: SBUS decode | [![planned #29](https://img.shields.io/badge/planned-%2329-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/29) <br />(stub today) | [![done #8](https://img.shields.io/badge/done-%238-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/8) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: barometer | [![done #23](https://img.shields.io/badge/done-%2323-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/23) <br />(BMP280) | [![done #23](https://img.shields.io/badge/done-%2323-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/23) <br />(DPS310) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: battery voltage/current sense | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) <br />(no PDB equivalent documented) | [![pending verify #24](https://img.shields.io/badge/pending_verify-%2324-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/24) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: IMU | [![done #27](https://img.shields.io/badge/done-%2327-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/27) <br />(MPU6500) | [![done #26](https://img.shields.io/badge/done-%2326-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/26) <br />(ICM42688P) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Servo: real PWM driver | [![done #31](https://img.shields.io/badge/done-%2331-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/31) | [![done #31](https://img.shields.io/badge/done-%2331-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/31) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: core (table + gather task) | [![done #16](https://img.shields.io/badge/done-%2316-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/16) <br />(compiles, no S.Port UART to put it on yet) | [![done #16](https://img.shields.io/badge/done-%2316-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/16) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: CRSF adapter | [![planned #19](https://img.shields.io/badge/planned-%2319-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/19) | [![planned #19](https://img.shields.io/badge/planned-%2319-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/19) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: S.Port adapter | [![planned #30](https://img.shields.io/badge/planned-%2330-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/30)/[![planned #46](https://img.shields.io/badge/planned-%2346-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/46) | [![pending verify #42](https://img.shields.io/badge/pending_verify-%2342-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/42) <br />(poll + Lua-push trim writes) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |

See [.docs/hardware.md](.docs/hardware.md) for the same matrix alongside
per-board MCU/RAM/flash specs, and [.docs/cli.md](.docs/cli.md) for the
CLI's own, more granular flag-level table.

## Repo structure (multi-target from the start)

- `boards/<target>/` — thin per-board layer: `board.c`/`board.h` (clock
  config, `board_init()`), `board_features.h` (capability flags),
  `FreeRTOSConfig.h`. Adding a target means adding a folder here, not
  touching anything else.
- `lib/` — chip/protocol-based drivers and hardware-independent logic,
  shared across every board that needs them (see [lib/README.md](lib/README.md)
  for why this is chip-based, not board-based — the axis
  `aoa-boat-controller` got wrong).
- `src/main.c` — single composition root for every board: `board_init()`,
  register modules (gated on `board_features.h`), start the scheduler.
- `vendor/freertos-kernel/` — vendored FreeRTOS-Kernel (Cortex-M3 and
  Cortex-M7 ports, both in use).

## Docs

- [.docs/hardware.md](.docs/hardware.md) — target boards, open hardware
  questions
- [.docs/cli.md](.docs/cli.md) — the USB-serial CLI: how it's wired, which
  boards have it, existing commands, how to add one
- [.docs/architecture/](.docs/architecture/) — architecture docs, split
  into one page per chain (receiver-to-servo, control loops, sensors,
  telemetry, logging) — design sketch, not yet implemented; start at
  [.docs/architecture/README.md](.docs/architecture/README.md)
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
package).

Bring-up and multi-target structure milestone: `matek_h743` and
`afroflight32` both build and link cleanly (real clock config from each
board's own bench-confirmed HSE/PLL values). `nexus_xr` is structurally
present but intentionally not buildable (see table above). `matek_h743`
is flashed and bench-verified on real hardware over USB DFU (`pio run -e
matek_h743 -t upload`, no ST-Link/CubeProgrammer needed); `afroflight32`
is flashed and bench-verified too, over its UART ROM bootloader (its CLI
was bench-verified this way in #13, fixing a VTOR + heap-exhaustion bug
in the process).

Past bring-up, the Input→Mapping→Control→Output→Servo chain (issue #7's
original stub) has real logic end to end on both buildable boards, not
just plumbing — each stage its own FreeRTOS task/queue, registered with
a module-liveness supervisor whose lifecycle/fault-isolation model is
now actually designed and implemented, not just sketched (see
[.docs/architecture/module-architecture.md](.docs/architecture/module-architecture.md);
only the exact numeric priority tiers remain open). On `matek_h743`, the
RX stage's SBUS driver does real UART/DMA decode rather than returning
fixed test data: USART6/PC7, DMA with idle-line detection, the STM32H7
hardware RX-invert bit instead of an external inverter — bench-verified
end to end with a real receiver and transmitter, both the explicit SBUS
failsafe bit and the receive-timeout backstop correctly reporting
`FAILSAFE` and recovering back to `OK`. `afroflight32`/`nexus_xr` don't
have a confirmed SBUS UART wiring yet ([#29](https://github.com/heimdall-suite/heimdall-helm/issues/29))
and still use the fixed-test-data stub; CRSF decode is a stub on every
board ([#9](https://github.com/heimdall-suite/heimdall-helm/issues/9)).
Mapping, output mapping (with per-slot failsafe/reverse/endpoint
trim), and the PWM servo driver are shared code that runs
identically on both boards.

Race-critical work for the 2026-09-24 event has landed on `main` since,
pending bench-verification (this repo's convention keeps an issue open
until confirmed on real hardware, even once its code has merged):
params-backed, runtime-settable output trim
([#39](https://github.com/heimdall-suite/heimdall-helm/issues/39)),
battery voltage/current and GPS wired into the telemetry table and
S.Port adapter ([#24](https://github.com/heimdall-suite/heimdall-helm/issues/24)/[#40](https://github.com/heimdall-suite/heimdall-helm/issues/40)/[#41](https://github.com/heimdall-suite/heimdall-helm/issues/41)),
and S.Port bidirectional Lua-push trim writes
([#42](https://github.com/heimdall-suite/heimdall-helm/issues/42)) — all
`matek_h743`-only so far, `afroflight32` parity tracked separately
([#46](https://github.com/heimdall-suite/heimdall-helm/issues/46)). See
[.docs/hardware.md](.docs/hardware.md) for the full feature/board matrix.

Real per-axis control-loop math (PID/attitude hold) is now tracked, not
just described: attitude estimation
([#49](https://github.com/heimdall-suite/heimdall-helm/issues/49)),
real Pitch PID
([#50](https://github.com/heimdall-suite/heimdall-helm/issues/50)), and
Roll hold as a genuinely new axis
([#51](https://github.com/heimdall-suite/heimdall-helm/issues/51),
architecture not yet decided) — informed by real prior-art investigation
on `aoa-boat-controller`, a comparable nitro-engine boat, see those
issues for what ported cleanly and what's boat-specific. That, a
genuinely configurable params-backed function/output mapping table
(today's is hardcoded, proving the mechanism), and blackbox logging
([#43](https://github.com/heimdall-suite/heimdall-helm/issues/43)/[#44](https://github.com/heimdall-suite/heimdall-helm/issues/44)/[#45](https://github.com/heimdall-suite/heimdall-helm/issues/45))
are still future work.
