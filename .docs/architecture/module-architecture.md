# Module / task architecture

Status: implemented — module lifecycle, pull-only queues, and the
supervisor/fault-isolation model below are real and bench-verified on
`matek_h743` and `afroflight32` (`nexus_xr` blocked on hardware, see
[.docs/hardware.md](../hardware.md)). Only the exact numeric priority
tiers (see "Open / not yet decided" below) remain undecided. See
[README.md](README.md) for how this page fits with the rest of the
architecture docs. Unlike the other pages here, this one isn't a signal
chain — it's the cross-cutting convention every chain (starting with
[receiver-to-servo.md](receiver-to-servo.md)) is built on top of.

## Module lifecycle

A module exposes a single `<module>_start(void)`, called once from
`src/main.c`, gated behind that module's `HELM_FEATURE_*` flag — this is
already the pattern `cli_start()` and `debug_heartbeat_start()` follow,
just formalized here rather than left implicit in `main.c`'s comments. A
board that doesn't budget for a feature simply never calls that module's
`_start()`, rather than every module carrying its own scattered
per-board `#ifdef`.

`_start()` does two things: create the module's task(s), and register the
module with the supervisor (see below) — an expected max update period
and a safe fallback value.

## Inter-stage data: pull, not push

Each module publishes its output as `{value, status}` into a **queue of
length 1**, written with `xQueueOverwrite`/`xQueueOverwriteFromISR`. This
always succeeds and always replaces whatever's currently there, read or
not — there is nothing to size and no overflow is possible by
construction, because a fast producer simply overwrites intermediate
values before a slower consumer gets to them. That's correct for latest-
value semantics: a consumer wants current state, not a backlog of stale
frames.

The next stage reads with `xQueuePeek` — never `xQueueReceive` — so the
value stays put for itself next tick and for any other consumer fanning
out from the same slot (e.g. `SENS -.-> CTRL` and `SENS -.-> TEL` reading
the same sensor queue independently). Each module exposes a
`<module>_get_latest(...)` accessor wrapping the peek, same naming
convention as `_start()`.

A module's container never calls the next stage directly. Direct calls
would collapse the pipeline into one call stack, tie every stage's timing
to whichever task started the chain, and break the supervisor's fallback
trick (below), which depends on downstream never being able to tell a
supervisor-injected value from a real one — only true if downstream is
always pulling from the same shared slot.

**This pattern is for control-state edges only** (Input to mapping,
Sensors to Control). It does not fit `SENS -.-> LOG` — blackbox logging
needs every sample, not just the latest, so overwrite-length-1 semantics
would silently drop the data logging exists to capture. That edge needs
its own multi-item queue (or ring buffer) with a real backpressure/drop
policy, once logging is actually designed (see
[logging.md](logging.md), currently a stub).

## Fault isolation: one supervisor, one status

[receiver-to-servo.md](receiver-to-servo.md) already designs a two-tier
`OK`/`FAILSAFE` status for RX, substituted at the mapping stage. This
generalizes to every module, not just RX: a lightweight supervisor task
holds a small table of registered modules (from `_start()`), each with a
liveness stamp the module's own task touches every pass, its declared max
update period, and its safe fallback value.

If a module goes quiet past its period — decode stalled, a task wedged,
anything — the supervisor overwrites *that module's own output queue*
with its fallback value and `FAILSAFE` status: the exact same mechanism a
dead RX link already triggers via its timeout watchdog. Downstream code
never needs to know *why* something failed, only that it did — one
mechanism, one status tag, whether the cause is a dead receiver link, a
lost sensor, or a wedged task.

## Priority tiers

Currently every task in the tree runs at priority 1 (`configMAX_PRIORITIES`
is 7 on all three boards, 0 reserved for idle, 4 reserved by FreeRTOS for
`configTIMER_TASK_PRIORITY`). That's fine for a heartbeat and a CLI; it
won't be once real modules exist. Tiers, highest to lowest:

1. Time-critical I/O — RX decode, servo output. Has to keep up with
   frame/PWM timing or the "latest value" it hands downstream goes stale.
2. Control loops.
3. Best-effort — telemetry, logging, CLI, heartbeat.

The supervisor's own tier, and exact numeric priorities per tier, aren't
decided yet — flagged as open rather than guessed at.

## Crash safety

On an unrecoverable fault (stack overflow, malloc failure), the crash
hooks in `src/main.c` currently `__disable_irq()` and halt forever, which
freezes PWM outputs at whatever duty cycle they last had — not a safe
one. Decided: don't try to force outputs safe from what may be a
corrupted context. Instead, an independent hardware watchdog (IWDG) is
fed only from the supervisor's own healthy pass; the crash hooks simply
stop feeding it (or explicitly starve it) instead of trying to clean up,
and let IWDG reset the MCU. A wedged task that stops the supervisor's
pass from running has the same effect. `board_init()` / output init are
responsible for coming up in a safe state on reboot.

Implemented on [![bench-verified #5](https://img.shields.io/badge/bench--verified-%235-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/5) `matek_h743` and [![bench-verified #11](https://img.shields.io/badge/bench--verified-%2311-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/11) `afroflight32`:
`lib/supervisor/supervisor.c`'s task starts and feeds IWDG itself, gated
on `HELM_HAS_IWDG`; the register-level init/refresh calls it makes live
in each board's own `board.c`, same standard as every other clock/
peripheral decision in this repo — the two implementations differ in
their prescaler/reload numbers (different nominal LSI frequency per
chip family) despite sharing the same register shape. Both
bench-verified via the CLI's `diag wedge` (`lib/diag/diag.c`): starving
the supervisor and confirming the board actually resets. ![not ported](https://img.shields.io/badge/not_ported-lightgrey)
on `nexus_xr` (STM32F7, still blocked on its hardware `#error`) —
`HELM_HAS_IWDG` is 0 there.

## Case study: Input (RX), dual-protocol selection

Input needs to support both SBUS and CRSF, runtime-selectable via a
persisted parameter (`HELM_FEATURE_PARAMS_PERSIST`, on for both real
boards as of #32; the persisted pick itself is #10's `PARAM_INPUT_MODE`,
`lib/params/params.c`). This is the direct precedent
[ports.md](ports.md)'s subsystem-owns-port-and-protocol model generalizes
from — every subsystem (`input`, `telemetry`, `gps`, `mag`, ...) gets the
same shape `rx_start()` already uses here, not just RX. This is a
different selection problem than
[lib/README.md](../../lib/README.md)'s existing chip-selection template,
worth calling out explicitly:

- **Hardware facts** (which IMU chip is wired) are genuinely build-time —
  a board only ever has one. `lib/README.md`'s existing
  `lib_ignore` + `custom_helm_<subsystem>` + one-driver-compiled-in
  template is correct for these; unchanged.
- **RX protocol is a user-facing mode**, not a hardware fact, over the
  same UART pin. So `lib/rx/sbus.c` and `lib/rx/crsf.c` both compile in,
  always, each implementing `rx.h`'s interface as a vtable
  (`const rx_driver_t *rx_sbus_driver(void)` /
  `rx_crsf_driver(void)` — `{init, poll}` function pointers, not
  `#ifdef`-selected free functions). `rx_start()` picks which vtable to
  bind at container-start time: the persisted input-mode param when
  params-persist is available, else a compile-time default
  (`HELM_RX_DEFAULT_PROTOCOL_SBUS` / `_CRSF` in `board_features.h`) — the
  fallback path *is* the "build flag to begin with," and it stays in
  place as the no-params-yet default (a board with
  `HELM_FEATURE_PARAMS_PERSIST` off) and as that same param's own
  factory-default value (`lib/params/params.c`) rather than being thrown
  away now that #10 has landed.

`lib/rx/shared/rx_timeout.c` (the receive-timeout backstop
[receiver-to-servo.md](receiver-to-servo.md) already designs) stays
unconditionally compiled, same as today's template describes for a
subsystem's `shared/` folder.

## Open / not yet decided

- Exact numeric priorities per tier, and where the supervisor sits.
- Supervisor poll period — detection latency vs. overhead tradeoff.
- The logging/telemetry multi-item queue pattern flagged above.
- Whether a wedged (not crashed) task should be individually recoverable
  (task restart) rather than only reachable via full IWDG reset — full
  reset is the only mechanism decided so far.
