# Architecture

Status: design sketch, not yet implemented — captures the intended shape
of the firmware before any driver/control-loop code gets written. See
repo root README's Status section for what actually exists today.

This is the overview. Each major chain gets its own page, added over time
as it gets designed — don't expect every page below to exist or be filled
in yet; a page that's just a stub says so at its own top.

```mermaid
%%{init: {'theme': 'redux', 'look': 'neo'}}%%
flowchart LR
    RX["Input<br/>SBUS/CRSF"] --> MAP["Function/input mapping"]
    MAP -->|"mode / target"| CTRL["Control loops"]
    MAP -->|"passthrough"| OUT["Output mapping"]
    CTRL --> OUT --> SERVO["Servo driver"]
    SENS["Sensors"] -.-> CTRL
    SENS -.-> LOG["Logging"]
    SENS -.-> TEL["Telemetry"]
```

## Pages

| Page | Covers |
|---|---|
| [receiver-to-servo.md](receiver-to-servo.md) | RX input, function/input mapping, failsafe, output mapping, servo driver |
| [control-loops.md](control-loops.md) | Per-axis control loops, mode transitions, timing/rate constraints |
| [sensors.md](sensors.md) | Sensor driver model, onboard vs. peripheral, hard-required vs. optional |
| [telemetry.md](telemetry.md) | Stub — reporting state back to the pilot's radio |
| [logging.md](logging.md) | Stub — blackbox/logging |

## Conventions for these pages

- Each page starts with its own `Status:` line — a page should be
  readable on its own if someone lands on it directly, not only in order.
- Cross-reference sibling pages by relative link
  (`[sensors.md](sensors.md)`) rather than repeating their content.
- New chains get their own page here, added to the table above, instead
  of growing inside an existing page once they're substantial enough to
  stand alone — this doc started as one file and got split for exactly
  that reason.
