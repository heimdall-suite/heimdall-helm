# Hardware

## Target boards — three, supported from the start

Committed targets, each with its own `boards/<target>/` folder and
`[env:...]` in `platformio.ini` (see that file's own comments for the
multi-target build approach):

| Target | MCU | Core/FPU | RAM / Flash | Status |
|---|---|---|---|---|
| `matek_h743` | STM32H743VIT6 | Cortex-M7, double-precision FPU | 512KB / 2MB | [![bench-verified #7](https://img.shields.io/badge/bench--verified-%237-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/7) <br />real clock config (8MHz HSE, 480MHz PLL), full bring-up chain |
| `afroflight32` | STM32F103CB | Cortex-M3, no FPU | 20KB / 128KB | [![bench-verified #13](https://img.shields.io/badge/bench--verified-%2313-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/13) <br />real clock config (12MHz HSE, 72MHz PLL), deliberately reduced feature set — see `boards/afroflight32/board_features.h` |
| `nexus_xr` | STM32F722RET6 | Cortex-M7, single-precision FPU | 256KB / 512KB | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) <br />structural placeholder — `board.c` intentionally `#error`s, no confirmed HSE/pin data yet |

## Feature / board matrix

Which subsystem is real (compiled with a bench-confirmed driver, not the
fixed-test-data/stub fallback) on which board. Badge color: green =
closed issue, bench-verified on real hardware; yellow = code merged to
`main` but the issue stays open pending bench-verification (this
project's convention — see repo root README's Status section); blue =
open issue, no code yet; grey = not applicable / blocked on hardware.

| Subsystem | `matek_h743` | `afroflight32` | `nexus_xr` |
|---|---|---|---|
| Control: attitude estimation (pitch + roll) | [![planned #49](https://img.shields.io/badge/planned-%2349-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/49) | [![planned #49](https://img.shields.io/badge/planned-%2349-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/49) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Pitch mode plumbing (placeholder passthrough) | [![done #35](https://img.shields.io/badge/done-%2335-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/35)&nbsp;[![pending verify #38](https://img.shields.io/badge/pending_verify-%2338-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/38) | [![done #35](https://img.shields.io/badge/done-%2335-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/35)&nbsp;[![pending verify #38](https://img.shields.io/badge/pending_verify-%2338-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/38) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Pitch real PID/attitude-hold | [![planned #50](https://img.shields.io/badge/planned-%2350-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/50) | [![planned #50](https://img.shields.io/badge/planned-%2350-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/50) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Roll hold (new axis, architecture TBD) | [![planned #51](https://img.shields.io/badge/planned-%2351-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/51) | [![planned #51](https://img.shields.io/badge/planned-%2351-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/51) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: CLI | [![done #1](https://img.shields.io/badge/done-%231-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/1) | [![done #13](https://img.shields.io/badge/done-%2313-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/13) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: IWDG crash safety | [![done #5](https://img.shields.io/badge/done-%235-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/5) | [![done #11](https://img.shields.io/badge/done-%2311-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/11) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: params persistence (generic store) | [![done #32](https://img.shields.io/badge/done-%2332-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/32) | [![done #32](https://img.shields.io/badge/done-%2332-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/32) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: ROM bootloader `dfu` jump | [![done #1](https://img.shields.io/badge/done-%231-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/1)/[![#3](https://img.shields.io/badge/done-%233-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/3) <br />(USB DFU) | [![done #28](https://img.shields.io/badge/done-%2328-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/28) <br />(UART/AN3155) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Logging: blackbox | [![planned #44](https://img.shields.io/badge/planned-%2344-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/44) <br />(SD card, storage backend not started) | [![planned #45](https://img.shields.io/badge/planned-%2345-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/45) <br />(SPI NOR, storage backend not started) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Mapping: function/input | [![done #34](https://img.shields.io/badge/done-%2334-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/34) | [![done #34](https://img.shields.io/badge/done-%2334-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/34) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Output: mapping (failsafe/reverse/endpoint) | [![done #36](https://img.shields.io/badge/done-%2336-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/36)&nbsp;[![done #37](https://img.shields.io/badge/done-%2337-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/37) | [![done #36](https://img.shields.io/badge/done-%2336-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/36)&nbsp;[![done #37](https://img.shields.io/badge/done-%2337-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/37) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Output: params-backed trim | [![pending verify #39](https://img.shields.io/badge/pending_verify-%2339-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/39) | [![pending verify #39](https://img.shields.io/badge/pending_verify-%2339-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/39) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Peripheral: GPS — NMEA 0183 decode | [![pending verify #40](https://img.shields.io/badge/pending_verify-%2340-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/40) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Peripheral: magnetometer | [![planned #48](https://img.shields.io/badge/planned-%2348-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/48) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| RX: CRSF decode | [![planned #9](https://img.shields.io/badge/planned-%239-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/9) <br />(stub today, all boards) | [![planned #9](https://img.shields.io/badge/planned-%239-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/9) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| RX: SBUS decode | [![done #8](https://img.shields.io/badge/done-%238-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/8) | [![planned #29](https://img.shields.io/badge/planned-%2329-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/29) <br />(stub today) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: barometer | [![done #23](https://img.shields.io/badge/done-%2323-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/23) <br />(DPS310) | [![done #23](https://img.shields.io/badge/done-%2323-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/23) <br />(BMP280) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: battery voltage/current sense | [![pending verify #24](https://img.shields.io/badge/pending_verify-%2324-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/24) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) <br />(no PDB equivalent documented) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: IMU | [![done #26](https://img.shields.io/badge/done-%2326-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/26) <br />(ICM42688P) | [![done #27](https://img.shields.io/badge/done-%2327-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/27) <br />(MPU6500) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Servo: real PWM driver | [![done #31](https://img.shields.io/badge/done-%2331-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/31) | [![done #31](https://img.shields.io/badge/done-%2331-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/31) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: core (table + gather task) | [![done #16](https://img.shields.io/badge/done-%2316-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/16) | [![done #16](https://img.shields.io/badge/done-%2316-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/16) <br />(compiles, no S.Port UART to put it on yet) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: CRSF adapter | [![planned #19](https://img.shields.io/badge/planned-%2319-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/19) | [![planned #19](https://img.shields.io/badge/planned-%2319-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/19) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: S.Port adapter | [![pending verify #42](https://img.shields.io/badge/pending_verify-%2342-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/42) <br />(poll + Lua-push trim writes) | [![planned #30](https://img.shields.io/badge/planned-%2330-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/30)/[![planned #46](https://img.shields.io/badge/planned-%2346-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/46) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |

See [.docs/cli.md](cli.md) for the CLI's own, more granular
`HELM_FEATURE_*`/`HELM_HAS_*` flag table, and
[.docs/architecture/](architecture/) for how each of these fits into the
signal chain.

`matek_h743` and `afroflight32` both run existing custom firmware today —
`aoa-boat-controller` (a separate, narrower AoA-only project on the same
physical boards) — and this project's own clock configs were ported from
its bench-confirmed values rather than re-derived from scratch. See each
board's `board.c` for the full provenance comment.

### RadioMaster Nexus-XR (the `nexus_xr` target)

Not a transmitter — a flybarless-helicopter flight-controller board
(STM32F722RET6, ICM42688P IMU, SPL06-001 baro, 256Mb blackbox flash
W25N02KVZEIR, 9-pin servo header, 3 independent UART ports A/B/C, XR
variant adds an onboard dual-SX1281 ExpressLRS receiver on its own UART5).
No unit on the bench yet, no confirmed HSE crystal value, no bench-verified
pin map — `boards/nexus_xr/board.c` deliberately refuses to guess (see
that file). Don't fill in real values without real hardware or a real
schematic to confirm against.

## Toolchain (decided)

PlatformIO, `framework = stm32cube` (raw HAL/LL, no Arduino) + FreeRTOS
(vendored, see [vendor/freertos-kernel](../vendor/freertos-kernel) —
Cortex-M3 and Cortex-M7 ports both vendored, since both are in active use
across the three targets). See repo root README's Status section for the
current milestone.

## Open items / not yet decided

- `afroflight32`'s battery-sense and GPS parity with `matek_h743` —
  no PDB-equivalent voltage/current sense documented yet, and no GPS
  header wired; tracked together as
  [#46](https://github.com/heimdall-suite/heimdall-helm/issues/46)
- `afroflight32`'s exact feature cuts — `board_features.h` has first-pass
  placeholder values marked TODO, not final decisions
- Real per-axis control-loop math (PID/attitude hold) — tracked as
  [#49](https://github.com/heimdall-suite/heimdall-helm/issues/49)
  (attitude estimation)/[#50](https://github.com/heimdall-suite/heimdall-helm/issues/50)
  (Pitch)/[#51](https://github.com/heimdall-suite/heimdall-helm/issues/51)
  (Roll, architecture TBD), not started — and CRSF decode/telemetry
  ([#9](https://github.com/heimdall-suite/heimdall-helm/issues/9)/[#19](https://github.com/heimdall-suite/heimdall-helm/issues/19)),
  also not started; see the feature matrix above and repo root README's
  Status section
- Exact numeric priority tiers for the module/scheduler architecture —
  the lifecycle/supervisor/fault-isolation model itself IS designed and
  bench-verified (see
  [.docs/architecture/module-architecture.md](architecture/module-architecture.md)),
  only the tier numbers remain open
- `nexus_xr`'s real HSE crystal value and pin map — needs real hardware or
  a real schematic, not more inference from spec sheets
- ST-Link/CubeProgrammer is not used for any target, by design — both
  real boards flash over their own ROM bootloaders instead (see
  [.docs/cli.md](cli.md)'s `dfu` command). Further real-hardware bring-up
  (`nexus_xr`, `afroflight32` battery/GPS/S.Port parity, blackbox
  storage) still needs to happen on the user's bench as each lands
