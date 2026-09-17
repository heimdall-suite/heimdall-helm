# Hardware

## Target boards (bench)

Existing custom firmware already runs on these — the plan is to harvest
working parts from that firmware, not port it wholesale:

- **Afroflight32 Acro Rev6** — STM32F103-class board.
- **Matek H743-WLITE** — STM32H743-class board (integrated Wi-Fi).

Multiple targets stay in scope long-term; these two are the current bench
set, not a final list.

## Preferred future target: RadioMaster Nexus-XR

Not a transmitter — a flybarless-helicopter flight-controller board
(STM32F722RET6, ICM42688P IMU, SPL06-001 baro, 256Mb blackbox flash
W25N02KVZEIR, 9-pin servo header, 3 independent UART ports A/B/C, XR
variant adds an onboard dual-SX1281 ExpressLRS receiver on its own UART5).
Preferred target hardware for this firmware to eventually run on, but the
firmware should not be hard-locked to it — other board targets stay in
scope.

## Toolchain (decided)

PlatformIO, `framework = stm32cube` (raw HAL/LL, no Arduino) + FreeRTOS
(vendored, see [vendor/freertos-kernel](../vendor/freertos-kernel)). See
repo root README's Status section for the current bring-up milestone.

## Open items / not yet decided

- Exact peripheral set carried over from the bench firmware vs. built fresh
- Whether STM32F103 (Afroflight32) stays a supported target long-term, or
  the project standardizes on H7-class parts (Matek H743-WLITE and later
  boards) once memory/compute needs from the four core features
  (sensors, logging, params, control loop) are clearer — Afroflight32's
  128KB flash / 20KB RAM is tight for FreeRTOS plus a full module set
- Module/scheduler architecture (task boundaries, fault isolation/HA model)
  — not designed yet, the real next step after this bring-up milestone
- Flashing/debug tooling (ST-Link/CubeProgrammer or similar) not available
  in the environment this was built in — real hardware bring-up and
  verification still needs to happen on the user's bench
