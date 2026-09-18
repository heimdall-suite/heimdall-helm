# Sensors

Status: design sketch, not yet implemented. See [README.md](README.md)
for how this page fits with the rest of the architecture docs.

Three independent consumers read sensor data, each at its own rate: a
[control loop](control-loops.md), [blackbox/logging](logging.md), and
[telemetry feedback](telemetry.md). This works because every driver
exposes a **latest value + status**, not a stream — see `lib/README.md`'s
driver convention. A slower consumer (logging, telemetry) just samples
"current state" less often than the control loop does; none of them need
every individual raw sample, matching `aoa-boat-controller`'s own
blackbox precedent (a fixed sampling rate, not raw capture).

**Onboard vs. peripheral** — two different presence models, not just a
labeling difference:
- **Onboard** (e.g. the board's own IMU/baro): presence is a *compile-time*
  fact — `board.c` always wires it up for that board type. This is what
  `board_features.h`'s `HELM_HAS_*` flags already model.
- **Peripheral** (e.g. an external compass on a free bus): presence is a
  *runtime* fact — the same board could have one attached or not. This
  wants boot-time detection (probe an address/`WHO_AM_I`, mark present or
  absent), not a board-level compile-time flag.

**Hard-required vs. optional/soft-dependency** — a third status tier
beyond a driver's own `OK`/`STALE`/`FAILED`. Example: the IMU is
hard-required for Pitch/Roll control — missing it means that control loop
can't run meaningfully, and should degrade toward passthrough. An external
compass is optional — it *grounds* the yaw axis (corrects for gyro drift
over time), but a yaw-holding loop still functions without one, just with
more drift, no fallback needed. This distinction lives in the *consumer*
(the control loop decides how much it needs a given input), not in the
driver — drivers stay dumb and report a generic status regardless of who's
reading them or how critical they are to that reader.

See [control-loops.md](control-loops.md)'s Timing/rates section for how
sensor read/fusion rate relates to (and is decoupled from) control loop
execution rate.
