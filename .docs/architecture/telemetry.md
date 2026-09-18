# Telemetry

Status: stub — not designed yet. See [README.md](README.md) for how this
page fits with the rest of the architecture docs.

What's established so far, from other pages: telemetry is one of three
independent consumers of [sensor data](sensors.md) (alongside the control
loop and [logging](logging.md)), reading the same latest-value+status
driver interface at its own rate. It also carries the diagnostic
frame-loss/link-quality data [receiver-to-servo.md](receiver-to-servo.md)'s
Failsafe section describes (not control-relevant, but useful to report
back to the pilot).

Not yet decided: which outbound protocol(s) (S.Port, CRSF telemetry, per
board — see `boards/*/board_features.h`), what gets reported and at what
rate, and how it relates to `aoa-boat-controller`'s `aoamon.lua`-style
parameter/config-over-telemetry precedent.
