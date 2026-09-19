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
| `matek_h743` | STM32H743 (Cortex-M7, dual-precision FPU) | Flashed/bench-verified over USB DFU; real SBUS decode bench-verified (see Status) |
| `afroflight32` | STM32F103 (Cortex-M3, no FPU) | Builds, real clock config, deliberately reduced feature set (20KB RAM); not yet flashed |
| `nexus_xr` | STM32F722 (Cortex-M7, single-precision FPU) | Structural placeholder only — `board.c` intentionally `#error`s, no confirmed pin/clock data exists yet |

See [.docs/hardware.md](.docs/hardware.md) for details, and
[boards/](boards/) for the per-target wiring/clock config/feature flags
themselves.

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
isn't flashed yet.

Past bring-up, a stub Input→Mapping→Control→Output→Servo chain runs on
`matek_h743` (each stage its own FreeRTOS task/queue, registered with a
module-liveness supervisor), and the RX stage's SBUS driver now does real
UART/DMA decode rather than returning fixed test data: USART6/PC7, DMA
with idle-line detection, the STM32H7 hardware RX-invert bit instead of
an external inverter. Bench-verified end to end with a real receiver and
transmitter — the explicit SBUS failsafe bit (transmitter off) and the
receive-timeout backstop (receiver unplugged) both correctly report
`FAILSAFE` and correctly recover back to `OK` once the signal returns.
`afroflight32`/`nexus_xr` don't have a confirmed SBUS UART wiring yet and
still use the fixed-test-data stub.

The actual module/scheduler architecture (extensibility, fault
isolation/HA design, not just today's stub chain) is still undesigned —
confirm architecture direction before building further modules out.
