# Logging (blackbox)

Status: stub — not designed yet. See [README.md](README.md) for how this
page fits with the rest of the architecture docs.

What's established so far, from other pages: logging is one of three
independent consumers of [sensor data](sensors.md) (alongside the control
loop and [telemetry](telemetry.md)), reading the same latest-value+status
driver interface at its own rate — a fixed sampling rate, not raw
per-sample capture, matching `aoa-boat-controller`'s own blackbox
precedent.

Not yet decided: storage medium/format per board (`HELM_HAS_BLACKBOX_STORAGE`
in `boards/*/board_features.h`), sample rate, record layout, and how a
log gets pulled off afterward.
