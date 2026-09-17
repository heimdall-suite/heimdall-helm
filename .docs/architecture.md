# Architecture: receiver-to-servo data flow

Status: design sketch, not yet implemented — captures the intended shape
of the pipeline before any driver/control-loop code gets written. See
repo root README's Status section for what actually exists today.

```mermaid
flowchart LR
    RX["Input<br/>SBUS/CRSF"]
    MAP["Function/input mapping<br/>(failsafe substitution happens here)"]
    CTRL["Control loops<br/>Pitch and/or Roll"]
    OUT["Output mapping"]
    SERVO["Servo driver"]
    SENS["Sensors<br/>onboard: IMU, baro<br/>peripheral: compass, ..."]
    LOG["Blackbox / logging"]
    TEL["Telemetry feedback"]

    RX --> MAP
    MAP -->|"mode / target functions"| CTRL
    MAP -->|"passthrough channels"| OUT
    CTRL --> OUT
    OUT --> SERVO

    SENS -.->|"latest value + status,<br/>each consumer reads at its own rate"| CTRL
    SENS -.-> LOG
    SENS -.-> TEL
```

Passthrough channels skip the control loops entirely — they go straight
from input mapping to output mapping. Only channels mapped to a mode or
target function feed a control loop.

## Function/input mapping

Assigns semantic meaning to each raw RX channel — configurable, not
hardcoded. A channel is either a straight **passthrough** (raw value flows
through unmodified) or a named **function** (a mode switch, or a target/
setpoint value feeding a control loop).

Example table:

| Channel | Mapping |
|---|---|
| CH1 | Passthrough |
| CH2 | Pitch mode `[Off / Limit / Active]` |
| CH3 | Passthrough |
| CH4 | Pitch target |
| CH5 | Passthrough |
| CH6 | Roll mode `[Off / Active]` |
| CH8 | Passthrough |
| ... | ... |

This is the layer that makes RX pluggable (SBUS vs. CRSF) invisible to
everything downstream — control loops and output mapping only ever see
named functions/passthroughs, never raw channel numbers or a specific
protocol's framing.

## Failsafe

Substitution happens **at the input mapping stage**, nowhere else:

- **Not in the RX driver** (`sbus.c`/`crsf.c`) — a protocol driver
  shouldn't know that CH2 means "Pitch mode." It only reports channels +
  an honest signal status.
- **Not in the servo driver** — it stays dumb, "doesn't know or care"
  whether a value came from passthrough or a control loop (see below). It
  has no basis to pick a safe value per physical servo, and doing it there
  wouldn't stop a control loop from meanwhile chasing a stale target.
- **At the mapping stage**, because that's the only place that knows both
  the RX signal status *and* what each channel semantically means. Each
  mapped function carries its own failsafe value: a mode function forces
  to a safe mode (most likely `Off`), a target function forces to a safe
  setpoint, a passthrough channel forces to a configured safe position
  (hold-last vs. a fixed preset — not decided yet).

Because substitution happens exactly once, before anything fans out,
**nothing downstream needs failsafe-awareness at all** — control loops,
output mapping, and the servo driver just run their completely normal
logic against whatever the mapping stage now presents. A mode forced to
`Off` during failsafe produces no output using the exact same "Off
produces no output" behavior that already exists for a pilot deliberately
selecting `Off`.

**RX status needs (at least) three tiers, not two**, because SBUS and
CRSF detect signal loss differently:
- `OK` — valid, current frame.
- `FRAME_LOST` — a brief gap. SBUS carries this as an explicit in-frame
  bit; CRSF has no equivalent bit and would need to infer it from a short
  receive timeout. Values should probably just hold, not trigger full
  failsafe substitution yet.
- `FAILSAFE` — latched loss. SBUS carries a second, distinct in-frame bit
  for this; CRSF has neither bit and relies entirely on a longer receive
  timeout. This is what triggers the per-function substitution above.

**No separate pipeline stage needed to normalize this** — `lib/rx/rx.h`'s
interface contract already guarantees `sbus.c` and `crsf.c` report the
same 3-tier status shape, regardless of how each arrives at it. The
mapping stage (and everything after it) never needs to know which
protocol is in use.

That said, the two drivers aren't fully independent in *how* they detect
loss, which is worth designing for even though it doesn't change the
external interface: **SBUS needs a receive-timeout backstop too, not just
its explicit bits.** The failsafe bit only tells you the *receiver*
detected its own RX-to-TX link is down — it says nothing about the wire
between the receiver and this board being cut, or the receiver itself
crashing/browning out, both of which mean no more frames arrive at all,
bit or no bit. So a generic "haven't seen a valid frame in N ms" watchdog
belongs once in `lib/rx/` as shared internal code, used by `crsf.c` as its
*only* detection mechanism and by `sbus.c` as a backstop layered under its
faster, bit-based detection. The bit-parsing itself stays entirely inside
`sbus.c` — CRSF has no equivalent, so there's nothing to share there.

Resuming: once RX status returns to `OK`, the mapping stage presumably
goes back to passing real values through immediately — no separate
"recovery" state currently planned, but worth confirming once this gets
built.

**Not yet decided**: the actual failsafe *values* — what mode is safe per
axis, what a passthrough channel's safe position should be, and whether
hold-last or a fixed preset is right for passthrough. That's a real
per-function, per-boat decision, not an architecture question — flagged
here as open rather than guessed at.

## Control loops

One control loop per controlled axis (Pitch, Roll, ...), each driven by
its own mode + target inputs from the mapping layer above, plus sensor
data (see below). Only channels mapped to a mode/target function reach
this stage at all — passthrough channels never touch a control loop (see
diagram above). A loop in `Off` mode produces no output.

**Mode transitions start clean, no bumpless transfer.** Settling time on
entering a mode is expected to be a few tenths of a second, which is an
acceptable transient — a loop doesn't need to carry state across a mode
change to avoid a servo "jump," it just zeroes and starts fresh.

## Sensors

Three independent consumers read sensor data, each at its own rate: a
control loop, blackbox/logging, and telemetry feedback. This works
because every driver exposes a **latest value + status**, not a stream —
see `lib/README.md`'s driver convention. A slower consumer (logging,
telemetry) just samples "current state" less often than the control loop
does; none of them need every individual raw sample, matching
`aoa-boat-controller`'s own blackbox precedent (a fixed sampling rate, not
raw capture).

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
as any other sensor consumer.

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

## Output mapping

Maps named signals — passthrough channels or a control loop's output — to
physical servo slots. Configurable the same way the input mapping is.

Example table:

| Servo | Source |
|---|---|
| S1 | CH1 |
| S2 | Pitch control output |
| S3 | CH3 |
| S4 | Roll control output |

## Servo driver

Takes the final per-servo values from the output mapping stage and drives
the actual PWM/output hardware. Doesn't know or care whether a given
servo's value came from passthrough or a control loop.
