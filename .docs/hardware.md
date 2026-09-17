# Hardware

## Target boards (bench)

Existing custom firmware already runs on these — the plan is to harvest
working parts from that firmware, not port it wholesale:

- **Afroflight32 Acro Rev6** — STM32F103-class board.
- **Matek H743-WLITE** — STM32H743-class board (integrated Wi-Fi).

Multiple targets stay in scope long-term; these two are the current bench
set, not a final list.

## Preferred transmitter

RadioMaster Nexus XR is the preferred target transmitter, but the firmware
should not be hard-locked to it — other transmitter/receiver links stay in
scope.

## Open items / not yet decided

- Exact peripheral set carried over from the bench firmware vs. built fresh
- Whether STM32F103 (Afroflight32) stays a supported target long-term, or
  the project standardizes on H7-class parts (Matek H743-WLITE and later
  boards) once memory/compute needs from the four core features
  (sensors, logging, params, control loop) are clearer
- Toolchain (bare-metal HAL, ChibiOS, Zephyr, or basing on an existing
  open-source FC stack like ArduPilot/PX4) — see repo root README Status
