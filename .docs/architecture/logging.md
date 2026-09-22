# Logging (blackbox)

Status: stub — no `lib/blackbox/` code exists yet, only planning issues:
[![planned #43](https://img.shields.io/badge/planned-%2343-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/43)
(shared log format + IMU capture task + enable/disable input mapping),
[![planned #44](https://img.shields.io/badge/planned-%2344-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/44)
(`matek_h743` SD-card storage backend), and
[![planned #45](https://img.shields.io/badge/planned-%2345-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/45)
(`afroflight32` SPI NOR storage backend). `HELM_FEATURE_BLACKBOX` is
already set to 1 in both real boards' `board_features.h` -- that's
declared intent for when this lands, not current capability. See
[README.md](README.md) for how this page fits with the rest of the
architecture docs.

What's established so far, from other pages: logging is one of three
independent consumers of [sensor data](sensors.md) (alongside the control
loop and [telemetry](telemetry.md)), reading the same latest-value+status
driver interface at its own rate — a fixed sampling rate, not raw
per-sample capture, matching `aoa-boat-controller`'s own blackbox
precedent.

Not yet decided: storage medium/format per board (`HELM_HAS_BLACKBOX_STORAGE`
in `boards/*/board_features.h`), sample rate, record layout, and how a
log gets pulled off afterward.
