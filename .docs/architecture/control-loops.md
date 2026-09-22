# Control loops

Status: mixed. The Off/Active mode-passthrough behavior described below
is real, shared code
([![pending verify #38](https://img.shields.io/badge/pending_verify-%2338-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/38),
race-critical for 2026-09-24, bench-verify pending). Real per-axis
PID/attitude-hold control-law math does not exist yet — `control.c`'s
Pitch loop is still a placeholder passthrough of its target (issue #35)
— but it's now tracked, not just described here: attitude estimation
([![planned #49](https://img.shields.io/badge/planned-%2349-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/49),
a prerequisite for both axes), real Pitch PID
([![planned #50](https://img.shields.io/badge/planned-%2350-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/50)),
and Roll hold as a genuinely new axis
([![planned #51](https://img.shields.io/badge/planned-%2351-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/51),
architecture not yet decided — see that issue). See
[README.md](README.md) for how this page fits with the rest of the
architecture docs.

One control loop per controlled axis (Pitch, Roll, ...), each driven by
its own mode + target inputs from [receiver-to-servo.md](receiver-to-servo.md)'s
function/input mapping stage, plus sensor data (see
[sensors.md](sensors.md)). Only channels mapped to a mode/target function
reach a control loop at all — passthrough channels never touch one. A
loop in `Off` mode produces no *correction* — the mapped raw target still
flows through to output mapping's own endpoint/subtrim/direction
trim (#37, #38), same as a passthrough channel would, just with
zero control-law contribution on top. This is deliberately different
from a genuine receiver failsafe (which still freezes/substitutes at the
output stage, per `receiver-to-servo.md`'s "Failsafe" section) — "off"
and "failsafe" stay two distinct states, not aliases of each other.

**Mode transitions start clean, no bumpless transfer.** Settling time on
entering a mode is expected to be a few tenths of a second, which is an
acceptable transient — a loop doesn't need to carry state across a mode
change to avoid a servo "jump," it just zeroes and starts fresh.

## Timing / rates

The control loop's **output** rate is capped around 400Hz — driven by the
actuator, not arbitrary. A standard PWM hobby servo's real mechanical
bandwidth is well below that (a full-scale move in ~2.5ms is faster than
any loaded servo gear train actually tracks), so computing/writing a new
commanded position faster than that spends scheduler budget the actuator
can't express anyway.

**Sensor read/fusion rate is architecturally decoupled from that output
rate** — but only sensor read/fusion, not the PID computation itself. A
faster attitude estimate (reading the IMU and running fusion more often
than 400Hz) can reduce lag/noise in the estimate without touching how
often the control loop commits a new servo command — the PID stage just
reads whatever the latest fused estimate is, same "latest value" pattern
as any other sensor consumer (see [sensors.md](sensors.md)).

The PID computation itself, including the **I-term's error accumulation,
must run at the same rate as the output commit** (≤400Hz), not faster.
Integrating error every time the I-term updates only makes sense if a
correction was actually applied to the physical system since the last
update — running the I-term faster than the actuator's own output rate
would accumulate error across cycles where nothing was actually corrected
yet, inflating it against a correction that hasn't happened. So: sensor
fusion rate is free to be faster; the control loop's own execution rate
(P, I, D, and the output write) is not.

Whether splitting fusion from PID execution is worth the complexity at all
is an open, empirical question (in the same vein as `aoa-boat-controller`'s
pitch-vibration investigation) — the architecture has to allow it from the
start rather than assume one shared rate everywhere, not that it has to be
built that way immediately.

## Tuning: gains and filters

Two different things need to be tunable, in two different places, not one
generic "tuning" bucket:

- **PID gains** (P/I/D per axis) live in the control loop itself (#50/#51).
- **Filter cutoffs / accel-trust-gate thresholds** live in the attitude
  estimator (#49), not here — `aoa-boat-controller`'s own architecture
  keeps these separate (`ControlLaw` has no filtering of its own;
  `AngleEstimator` owns all of it), worth keeping that split here too.

Both go through the existing params store (#32) as ordinary `ParamId`s,
the same way #39 made output trim runtime-settable — no new persistence
or tuning mechanism needs inventing. That also means both get live
S.Port Lua-push tuning for free via #42's existing write path, not just
`param set` over the CLI — a pattern `aoa-boat-controller` already proved
out for exactly this use case, not just a theoretical benefit: its
`SportCommand` protocol has a real `kSetPidGains` command, and its
`aoamon.lua` EdgeTX Config page has live P/I/D fields with a "Send PID"
push button, confirmed in the owner's own real, current use on a real
transmitter. This codebase's own #42 implements the same push mechanism
at the transport level already; wiring gains/filter cutoffs onto it via
params (#32) is the remaining work, not inventing a new mechanism.

**On notch filters specifically**: don't assume one is needed, or even
desirable, without first characterizing this boat's real vibration
spectrum on the bench (FFT across throttle levels, gyro + accel). Real
prior investigation on a comparable nitro-engine boat
(`aoa-boat-controller`'s `docs/pitch-vibration-investigation.md`)
explicitly considered and rejected a gyro-rate notch filter for its pitch
axis — the frequency a notch would have targeted turned out to be the
hull's own real porpoising motion, exactly the signal the control loop
needs to react to, not noise. They shipped a plain single-pole low-pass
on the *accel* path only, tuned to pass that reaction band while
rejecting genuinely higher-frequency engine vibration; gyro rate stayed
unfiltered since it feeds the D-term, where added latency is costlier
than on the accel path's slow drift correction. Whether a notch filter
ever makes sense here depends on whether this boat's own vibration
spectrum actually separates "real motion to react to" from "noise to
reject" the same way — a bench characterization question, not something
to design around in the abstract.
