# Hardware

## Target boards — three, supported from the start

Committed targets, each with its own `boards/<target>/` folder and
`[env:...]` in `platformio.ini` (see that file's own comments for the
multi-target build approach):

| Target | MCU | Core/FPU | RAM / Flash | Status |
|---|---|---|---|---|
| `matek_h743` | STM32H743VIT6 | Cortex-M7, double-precision FPU | 512KB / 2MB | Builds, real clock config (8MHz HSE, 480MHz PLL) |
| `afroflight32` | STM32F103CB | Cortex-M3, no FPU | 20KB / 128KB | Builds, real clock config (12MHz HSE, 72MHz PLL), deliberately reduced feature set — see `boards/afroflight32/board_features.h` |
| `nexus_xr` | STM32F722RET6 | Cortex-M7, single-precision FPU | 256KB / 512KB | Structural placeholder — `board.c` intentionally `#error`s, no confirmed HSE/pin data yet |

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

- Exact peripheral set per board (beyond the IMU each already has) — not
  designed yet, part of the module architecture below
- `afroflight32`'s exact feature cuts — `board_features.h` has first-pass
  placeholder values marked TODO, not final decisions
- Module/scheduler architecture (task boundaries, fault isolation/HA model)
  — not designed yet, the real next step after this milestone
- `nexus_xr`'s real HSE crystal value and pin map — needs real hardware or
  a real schematic, not more inference from spec sheets
- Flashing/debug tooling (ST-Link/CubeProgrammer or similar) not available
  in the environment this was built in — real hardware bring-up and
  verification still needs to happen on the user's bench
