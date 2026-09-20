# Receiver-to-servo flow

Status: design sketch, not yet implemented. See [README.md](README.md)
for how this page fits with the rest of the architecture docs.

```mermaid
%%{init: {'theme': 'redux', 'look': 'neo'}}%%
flowchart LR
    subgraph Input["Input (exactly one compiled per board)"]
        direction TB
        subgraph SBUS
            direction TB
            DSBUS["Decode<br/>(+ explicit frame-lost/<br/>failsafe bits)"]
            TOSBUS["Timeout watchdog<br/>(backstop)"]
            DSBUS --> OUTSBUS["channels + status"]
            TOSBUS --> OUTSBUS
        end
        subgraph CRSF
            direction TB
            DCRSF["Decode"]
            TOCRSF["Timeout watchdog<br/>(only mechanism —<br/>no in-frame bits)"]
            DCRSF --> OUTCRSF["channels + status"]
            TOCRSF --> OUTCRSF
        end
    end

    MAP["Function/input mapping<br/>(mode/target failsafe<br/>substitution happens here)"]
    CTRL["Control loops<br/>Pitch and/or Roll"]
    OUT["Output mapping<br/>(passthrough failsafe<br/>substitution happens here)"]
    SERVO["Servo driver"]
    SENS["Sensors<br/>onboard: IMU, baro<br/>peripheral: compass, ..."]
    LOG["Blackbox / logging"]
    TEL["Telemetry feedback"]

    OUTSBUS --> MAP
    OUTCRSF --> MAP
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
target function feed a control loop (see [control-loops.md](control-loops.md)
for what happens there).

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

Split across two stages by what each one actually knows, not handled in
one place the way an earlier version of this doc described:

- **Not in the RX driver** (`sbus.c`/`crsf.c`) — a protocol driver
  shouldn't know that CH2 means "Pitch mode." It only reports channels +
  an honest signal status.
- **Mode/target functions: substituted at the input mapping stage**,
  because that's the only place that knows both the RX signal status
  *and* what a mapped channel semantically means. A mode function forces
  to a safe mode (most likely `Off`); a target function forces to a safe
  setpoint. A mode forced to `Off` during failsafe produces no output
  using the exact same "Off produces no output" behavior that already
  exists for a pilot deliberately selecting `Off` — control loops don't
  need any failsafe-awareness of their own for this path.
- **Passthrough channels: substituted at the output mapping stage
  instead**, per physical servo slot, each independently configured to
  either hold its last known value or snap to a fixed configured preset.
  Deliberate departure from "the mapping stage is the only place that
  knows enough": a passthrough channel carries no semantic meaning for
  input mapping to substitute *against* in the first place, and input
  mapping doesn't even know which physical slot(s) a passthrough channel
  will eventually land on (`lib/output/`'s own table decides that) — so
  "what's safe here" is a property of the physical actuator output
  mapping already owns the assignment for, not of the raw input channel.
  This does mean output mapping (and, for a control-loop-fed slot, that
  loop's own output) needs failsafe-awareness after all, unlike the
  "nothing downstream needs it" framing this section used to have for
  every path uniformly.

The **servo driver** stays failsafe-unaware either way — it doesn't know
or care whether a value came from passthrough or a control loop, and by
the time output mapping hands it a value, that value is already the
final, failsafe-substituted-if-needed one.

**Control-relevant status is only two tiers, `OK` vs. `FAILSAFE`** — a
brief frame-loss gap doesn't need its own behavioral branch, because
"hold the last known value" isn't something that needs code to make
happen: the driver simply doesn't update its latest-value struct on a
failed decode, so any consumer reading it already sees the previous good
values, automatically. There's no point where the mapping stage needs to
notice a gap and *decide* to hold — holding is just what "no update
happened" already looks like.

A finer-grained frame-loss signal (SBUS's explicit bit, or a receive-age
timer for CRSF) is real and useful, but as **diagnostic data for
logging** — not as a third status tier other logic branches on, and not
as telemetry content either: real link-quality/RSSI is the receiver's
own responsibility to report, independent of the FC, on both protocols
(see [telemetry.md](telemetry.md)'s "Link quality is not gathered here").
Keep it as a separate field (e.g. a last-frame-age or loss counter)
alongside the `OK`/`FAILSAFE` status, consumed by logging the same
"latest value" way sensor data is, rather than folding it into the value
that gates mapping-stage substitution.

**No separate pipeline stage needed to normalize any of this** —
`lib/rx/rx.h`'s interface contract already guarantees `sbus.c` and
`crsf.c` report the same shape, regardless of how each arrives at it. The
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
axis, what a passthrough channel's per-slot preset value should be. Hold-
last vs. fixed preset is no longer an either/or architecture question
(above: both exist, configured per physical slot), but which one a given
slot should actually use, and what the preset value is, is still a real
per-function, per-boat decision — flagged here as open rather than
guessed at.

## Output mapping

Maps named signals — passthrough channels or a control loop's output — to
physical servo slots. Configurable the same way the input mapping is.
Also where passthrough failsafe substitution happens (see "Failsafe"
above) — each physical slot fed by a passthrough channel carries its own
hold-last-vs-fixed-preset config, independent of every other slot.

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
