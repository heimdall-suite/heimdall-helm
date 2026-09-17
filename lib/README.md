# lib/

Empty for now — no real modules exist yet (see repo root README's Status
section). This is where they'll go, following one convention:

## Split by chip/protocol, not by board

`aoa-boat-controller` split drivers by *board* (`lib/Imu/H743` vs
`lib/Imu/Naze32`), which means every new target starts by copy-pasting a
working driver even when two boards share the exact same chip — true
today: the Matek H743-WLITE and the (future) Nexus-XR both plausibly carry
an ICM42688P-family IMU.

Instead, each subfolder here is one *subsystem*, containing one driver per
*chip* it supports — e.g. `lib/imu/icm42688p.c`, `lib/imu/mpu6500.c`,
sharing one `lib/imu/imu.h` interface. `boards/<target>/board.c` decides
which driver instance(s) a given board actually wires up; the driver code
itself is reused across every board carrying that chip.

## Keep each subsystem flat (one level)

PlatformIO's LDF only auto-scans the *immediate* children of `lib/` as
libraries — nesting a further split underneath (the way
`aoa-boat-controller` had to, e.g. `lib/SPort/H743/SportSensor`) silently
drops out of the build unless every nested folder is separately listed in
`lib_extra_dirs`, a trap that project's own `platformio.ini` documents
hitting. Splitting by chip instead of by board avoids the need for that
extra nesting level in the first place — keep it that way.

## More than one chip in a subsystem needs explicit selection, not auto-scan

A subsystem folder with more than one concrete implementation (e.g.
`lib/rx/sbus.c` and `lib/rx/crsf.c`, both implementing `rx.h`'s same
function names) can't be left to PlatformIO's default auto-scan at all —
the LDF isn't picky about which symbols a board actually needs, so it
would try to compile *every* file in that folder into *every* board and
collide at link time. `lib/rx/`'s own `platformio.ini` entries are the
template for this: `lib_ignore = <subsystem>` opts the folder out of
auto-scan entirely, and a small `scripts/add_<subsystem>.py` explicitly
compiles only the one implementation that board names (via a
`custom_helm_<subsystem>` option), plus that subsystem's `shared/`
subfolder (genuinely shared code — e.g. `lib/rx/shared/rx_timeout.c`,
used internally by every implementation) unconditionally. Follow this
template for `lib/imu/`, `lib/baro/`, and anything else that ends up with
more than one chip per subsystem — which, given the whole point of
splitting by chip, is the normal case, not an edge case.

## Hardware-independent logic lives here too

Genuinely target-independent code (control law math, filters, protocol
framing) belongs here as its own subsystem folder, not duplicated per
board — same as `aoa-boat-controller`'s `Shared/` folders, just without a
sibling per-board folder next to it unless a real hardware split exists.
