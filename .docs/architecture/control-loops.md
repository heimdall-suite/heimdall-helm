# Control loops

Status: design sketch, not yet implemented. See [README.md](README.md)
for how this page fits with the rest of the architecture docs.

One control loop per controlled axis (Pitch, Roll, ...), each driven by
its own mode + target inputs from [receiver-to-servo.md](receiver-to-servo.md)'s
function/input mapping stage, plus sensor data (see
[sensors.md](sensors.md)). Only channels mapped to a mode/target function
reach a control loop at all — passthrough channels never touch one. A
loop in `Off` mode produces no output.

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
