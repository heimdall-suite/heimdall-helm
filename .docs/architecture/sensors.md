# Sensors

Status: mostly implemented. IMU and baro are real, bench-verified
onboard drivers on both `matek_h743` and `afroflight32`
([#26](https://github.com/heimdall-suite/heimdall-helm/issues/26)/[#27](https://github.com/heimdall-suite/heimdall-helm/issues/27)/[#23](https://github.com/heimdall-suite/heimdall-helm/issues/23)).
Battery voltage/current and GPS are real on `matek_h743` only, pending
bench-verification for the 2026-09-24 race
([#24](https://github.com/heimdall-suite/heimdall-helm/issues/24)/[#40](https://github.com/heimdall-suite/heimdall-helm/issues/40));
`afroflight32` has no equivalent hardware yet
([#46](https://github.com/heimdall-suite/heimdall-helm/issues/46)). A
magnetometer ([#48](https://github.com/heimdall-suite/heimdall-helm/issues/48))
is backlog on `matek_h743` only — a peripheral like GPS, not something
either board has onboard, and no real module is in hand yet to build a
driver against; not planned for `afroflight32` at all, unlike GPS/battery's
[#46](https://github.com/heimdall-suite/heimdall-helm/issues/46) parity
tracking, given that board's tight RAM budget. See
[.docs/hardware.md](../hardware.md)'s feature matrix for the full
per-board breakdown, and [README.md](README.md) for how this page fits
with the rest of the architecture docs.

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
  absent), not a board-level compile-time flag. `lib/sensors/gps.c`
  (`matek_h743`, issue #40) is the first real driver built against this
  model: `HELM_HAS_GPS` only tracks that a UART is wired to a GPS header,
  not that a module is actually plugged in and powered, since the GPS
  needs external power the user connects on demand — `gps_get_latest()`
  reports `SENSOR_STATUS_FAILED` for as long as nothing answers, same as
  any other absent peripheral would. A magnetometer would be the second
  (issue #48, `matek_h743` only) — never an onboard chip, always an
  external compass module, same presence model as GPS.

**Hard-required vs. optional/soft-dependency** — a third status tier
beyond a driver's own `OK`/`STALE`/`FAILED`. Example: the IMU is
hard-required for Pitch/Roll control (see
[control-loops.md](control-loops.md)'s Status line for the attitude
estimation/Pitch/Roll issues, #49/#50/#51) — missing it means that
control loop can't run meaningfully, and should degrade toward
passthrough. An external
compass (issue #48) is optional — it *grounds* the yaw axis (corrects for
gyro drift over time), but a yaw-holding loop still functions without
one, just with more drift, no fallback needed. This distinction lives in
the *consumer*
(the control loop decides how much it needs a given input), not in the
driver — drivers stay dumb and report a generic status regardless of who's
reading them or how critical they are to that reader.

See [control-loops.md](control-loops.md)'s Timing/rates section for how
sensor read/fusion rate relates to (and is decoupled from) control loop
execution rate.
