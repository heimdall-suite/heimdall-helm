# Hardware

## Target boards — three, supported from the start

Committed targets, each with its own `boards/<target>/` folder and
`[env:...]` in `platformio.ini` (see that file's own comments for the
multi-target build approach):

| Target | MCU | Core/FPU | RAM / Flash | Status |
|---|---|---|---|---|
| `matek_h743` | STM32H743VIT6 | Cortex-M7, double-precision FPU | 512KB / 2MB | [![bench-verified #7](https://img.shields.io/badge/bench--verified-%237-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/7) <br />real clock config (8MHz HSE, 480MHz PLL), full bring-up chain |
| `afroflight32` | STM32F103CB | Cortex-M3, no FPU | 20KB / 128KB | [![bench-verified #13](https://img.shields.io/badge/bench--verified-%2313-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/13) <br />real clock config (12MHz HSE, 72MHz PLL), deliberately reduced feature set — see `boards/afroflight32/board_features.h` |
| `nexus_xr` | STM32F722RET6 | Cortex-M7, single-precision FPU | 256KB / 512KB | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) <br />structural placeholder — `board.c` intentionally `#error`s, no confirmed HSE/pin data yet |

## Feature / board matrix

Which subsystem is real (compiled with a bench-confirmed driver, not the
fixed-test-data/stub fallback) on which board. Badge color: green =
closed issue, bench-verified on real hardware; yellow = code merged to
`main` but the issue stays open pending bench-verification (this
project's convention — see repo root README's Status section); blue =
open issue, no code yet; grey = not applicable / blocked on hardware.

| Subsystem | `matek_h743` | `afroflight32` | `nexus_xr` |
|---|---|---|---|
| Control: attitude estimation (pitch + roll) | [![planned #49](https://img.shields.io/badge/planned-%2349-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/49) | [![planned #49](https://img.shields.io/badge/planned-%2349-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/49) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Pitch mode plumbing (placeholder passthrough) | [![done #35](https://img.shields.io/badge/done-%2335-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/35)&nbsp;[![pending verify #38](https://img.shields.io/badge/pending_verify-%2338-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/38) | [![done #35](https://img.shields.io/badge/done-%2335-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/35)&nbsp;[![pending verify #38](https://img.shields.io/badge/pending_verify-%2338-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/38) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Pitch real PID/attitude-hold | [![planned #50](https://img.shields.io/badge/planned-%2350-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/50) | [![planned #50](https://img.shields.io/badge/planned-%2350-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/50) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Control: Roll hold (new axis, architecture TBD) | [![planned #51](https://img.shields.io/badge/planned-%2351-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/51) | [![planned #51](https://img.shields.io/badge/planned-%2351-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/51) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: CLI | [![done #1](https://img.shields.io/badge/done-%231-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/1) | [![done #13](https://img.shields.io/badge/done-%2313-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/13) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: IWDG crash safety | [![done #5](https://img.shields.io/badge/done-%235-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/5) | [![done #11](https://img.shields.io/badge/done-%2311-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/11) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: params persistence (generic store) | [![done #32](https://img.shields.io/badge/done-%2332-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/32) | [![done #32](https://img.shields.io/badge/done-%2332-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/32) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Core: ROM bootloader `dfu` jump | [![done #1](https://img.shields.io/badge/done-%231-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/1)/[![#3](https://img.shields.io/badge/done-%233-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/3) <br />(USB DFU) | [![done #28](https://img.shields.io/badge/done-%2328-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/28) <br />(UART/AN3155) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Logging: blackbox | [![planned #44](https://img.shields.io/badge/planned-%2344-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/44) <br />(SD card, storage backend not started) | [![planned #45](https://img.shields.io/badge/planned-%2345-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/45) <br />(SPI NOR, storage backend not started) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Mapping: function/input | [![done #34](https://img.shields.io/badge/done-%2334-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/34) | [![done #34](https://img.shields.io/badge/done-%2334-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/34) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Output: mapping (failsafe/reverse/endpoint) | [![done #36](https://img.shields.io/badge/done-%2336-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/36)&nbsp;[![done #37](https://img.shields.io/badge/done-%2337-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/37) | [![done #36](https://img.shields.io/badge/done-%2336-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/36)&nbsp;[![done #37](https://img.shields.io/badge/done-%2337-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/37) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Output: params-backed trim | [![pending verify #39](https://img.shields.io/badge/pending_verify-%2339-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/39) | [![pending verify #39](https://img.shields.io/badge/pending_verify-%2339-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/39) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Peripheral: GPS — NMEA 0183 decode | [![pending verify #40](https://img.shields.io/badge/pending_verify-%2340-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/40) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Peripheral: magnetometer | [![planned #48](https://img.shields.io/badge/planned-%2348-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/48) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| RX: CRSF decode | [![planned #9](https://img.shields.io/badge/planned-%239-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/9) <br />(stub today, all boards) | [![planned #9](https://img.shields.io/badge/planned-%239-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/9) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| RX: SBUS decode | [![done #8](https://img.shields.io/badge/done-%238-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/8) | [![planned #29](https://img.shields.io/badge/planned-%2329-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/29) <br />(stub today) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: barometer | [![done #23](https://img.shields.io/badge/done-%2323-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/23) <br />(DPS310) | [![done #23](https://img.shields.io/badge/done-%2323-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/23) <br />(BMP280) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: battery voltage/current sense | [![pending verify #24](https://img.shields.io/badge/pending_verify-%2324-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/24) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) <br />(no PDB equivalent documented) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Sensor: IMU | [![done #26](https://img.shields.io/badge/done-%2326-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/26) <br />(ICM42688P) | [![done #27](https://img.shields.io/badge/done-%2327-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/27) <br />(MPU6500) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Servo: real PWM driver | [![done #31](https://img.shields.io/badge/done-%2331-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/31) | [![done #31](https://img.shields.io/badge/done-%2331-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/31) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: core (table + gather task) | [![done #16](https://img.shields.io/badge/done-%2316-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/16) | [![done #16](https://img.shields.io/badge/done-%2316-brightgreen)](https://github.com/heimdall-suite/heimdall-helm/issues/16) <br />(compiles, no S.Port UART to put it on yet) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: CRSF adapter | [![planned #19](https://img.shields.io/badge/planned-%2319-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/19) | [![planned #19](https://img.shields.io/badge/planned-%2319-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/19) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |
| Telemetry: S.Port adapter | [![pending verify #42](https://img.shields.io/badge/pending_verify-%2342-yellow)](https://github.com/heimdall-suite/heimdall-helm/issues/42) <br />(poll + Lua-push trim writes) | [![planned #30](https://img.shields.io/badge/planned-%2330-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/30)/[![planned #46](https://img.shields.io/badge/planned-%2346-blue)](https://github.com/heimdall-suite/heimdall-helm/issues/46) | ![not ported](https://img.shields.io/badge/not_ported-lightgrey) |

See [.docs/cli.md](cli.md) for the CLI's own, more granular
`HELM_FEATURE_*`/`HELM_HAS_*` flag table, and
[.docs/architecture/](architecture/) for how each of these fits into the
signal chain.

`matek_h743` and `afroflight32` both run existing custom firmware today —
`aoa-boat-controller` (a separate, narrower AoA-only project on the same
physical boards) — and this project's own clock configs were ported from
its bench-confirmed values rather than re-derived from scratch. See each
board's `board.c` for the full provenance comment.

### RadioMaster Nexus-XR (the `nexus_xr` target)

Not a transmitter — a flybarless-helicopter flight-controller board
(STM32F722RET6, ICM42688P IMU, SPL06-001 baro, 256Mb blackbox flash
W25N02KVZEIR, 9-pin servo header, 3 independent UART ports A/B/C, XR
variant adds an onboard dual-SX1281 ExpressLRS receiver on its own UART5).
No unit on the bench yet, no confirmed HSE crystal value, no bench-verified
pin map — `boards/nexus_xr/board.c` deliberately refuses to guess (see
that file). Don't fill in real values without real hardware or a real
schematic to confirm against.

## Port inventory

Per-board UART/I2C peripheral inventory backing the peripheral-routing
architecture ([.docs/architecture/ports.md](architecture/ports.md),
issues [#52](https://github.com/heimdall-suite/heimdall-helm/issues/52)-[#57](https://github.com/heimdall-suite/heimdall-helm/issues/57)).
Each port gets a uniform `Port <letter>` name, one letter space per board.

### nexus_xr

Sourced from [INAV's NEXUSX pinout doc](https://github.com/iNavFlight/inav/blob/master/docs/boards/NEXUSX.md).
⚠️ that doc itself flags that Radiomaster's own site has RX/TX swapped in
its A/B/C pin-order listing — INAV's corrected version is the one to trust.

**Ports A/B/C reuse this board's own physical silkscreen letters** —
confirmed directly against the doc's "Marking on the case" column: the
case itself is printed "A" next to the UART4 connector, "B" next to
UART6, "C" next to the UART3-or-I2C2 one, exactly matching the table
below. **Ports D and E do not exist as letters on the physical board —
they're this project's own naming, not silkscreen.** The case prints
"AUX"/"SBUS" where Port D is, and nothing at all where Port E is (an
internal, non-connectorized UART). Don't go looking for a "D" or "E"
printed on a real unit; use the case's own "AUX"/"SBUS" markings or the
peripheral name instead when working from hardware in hand.

| Port | Peripheral | Transport | Notes |
|---|---|---|---|
| A | UART4 | UART only | Free, generic candidate |
| B | UART6 | UART only | Free, generic candidate |
| C | UART3 *or* I2C2 | Alternate-function, genuinely exclusive (shared pins) | Needs a `.mode` field once params land (#54) |
| D | UART1 *or* I2C1 *or* Servo (case labels "AUX"/"SBUS", pins PB6/PB7) | 3-way alternate-function, genuinely exclusive (shared pins) | Resolves this section's old open item (was "needs its own name... UART1 may already be claimed — resolve, don't assume", see #52) — needs a `.mode` field once params land (#54), same as Port C. See note below for why this one's a 3-way choice, not 2-way |
| E | UART5 (PC12/PD2) | Onboard, not exposed to any connector — case prints "built-in ELRS" here, not "E" | Hardwired to the built-in dual-SX1281 ExpressLRS receiver (XR variant only — plain `NEXUS` has no such chip); matches `HELM_RX_DEFAULT_PROTOCOL_CRSF`. Not a port candidate, same category as `matek_h743`'s onboard I2C2 baro (Port I) — this is the `input` subsystem's `source = onboard` case in `ports.md`'s model, not `direct`+`.port` |

Port D resolves cleanly, not just gets a name: INAV's own "Pin
configuration" table shows PB6/PB7 default to plain servo outputs
(S8/S9) in every configuration where neither UART1 nor UART2 is
explicitly turned on in INAV's ports tab, and only become UART1 TX/RX
once that's done — nothing pre-claims them. So the old "may already be
claimed for RX" worry doesn't hold up: this is genuinely free hardware,
not a hidden default. The same doc's "Hardware layout" table confirms
PB6/PB7 double as I2C1 SCL/SDA too, matching Port C's shared-pins shape —
except here servo output is a real third mode, not just UART-or-I2C, so
whatever eventually reads `.mode` for this port needs a third case Port
C doesn't have. Worth flagging for whoever designs that: unlike Ports
A-C, "unclaimed" here isn't just "idle" — it's actively a servo output
pin pair (S8/S9) by this board's own default wiring, so the
output-mapping/servo-count system and this port's claim state will need
to agree on which one wins. Not decided by this issue — see "Open items"
below.

Also surfaced but out of scope, same `CAN1`/S.Port-softserial treatment
as `matek_h743`/`afroflight32`'s callouts above: INAV's "Hardware
layout" table separately lists PA9 (the "ESC" motor-output pin, one of
the plain S1-S5 servo pins, never a UART candidate in the actual
"Pin configuration" table) as *also* capable of carrying UART1 TX at the
chip level. Real chip fact, not a usable board feature — this board's
own pin-configuration table never exposes that option, only AUX/SBUS
ever become UART1. Not a Port.

Port E (UART5) is a straightforward onboard case, no alternate-function
complexity — INAV's own text is direct: "It is connected to the main
STM32F7 Flight Controller on UART5. None of the external connections
route to the receiver, they are all connected to the STM32F7 Flight
Controller." One caveat worth carrying forward: the same doc says the
receiver "can be disabled using USER1, which controls a pinio on pin
PC8" — that's a power/enable line for the ESP32-based receiver module,
not a pin remux, and nothing in the source says disabling it frees
UART5's PC12/PD2 pins for any other use. Don't assume it does without a
real source saying so.

### matek_h743

Sourced from [Matek's H743-WLITE product page](https://www.mateksys.com/?portfolio=h743-wlite#tab-id-5)
(manufacturer silkscreen, the primary source Betaflight's/INAV's own
target configs are themselves written against), cross-checked against
INAV's [`MATEKH743/target.h`](https://github.com/iNavFlight/inav/blob/master/src/main/target/MATEKH743/target.h).
Independently cross-validates the three UARTs already wired — GPS UART3
PD8/PD9 (#40), SBUS UART6 PC7 (#8), S.Port UART7 PE8 (#18) all match —
and resolves the `HELM_HAS_BARO` "verify wiring" TODO: I2C2/PB10-PB11 is
that bus. 7 UARTs + 2 I2C buses, one uniform letter space (A–I); I2C buses
get their own letters rather than folding into the UART lettering, since
their pins don't share a connector with any lettered UART on this board.
Silk labels below come from that same product page's pad-name column
("TX1 RX1", "TX2 RX2", etc.), cross-validated by appearing identically in
both its "INAV mapping" (`#tab-id-5`) and "ArduPilot mapping" (`#tab-id-6`)
tables — the same pad names under two unrelated firmware projects' own
port-role conventions, not something either one invented.

| Port | Peripheral | Pins | Silk label | Matek's suggested use (not silkscreen) | Notes |
|---|---|---|---|---|---|
| A | UART1 | PA9/PA10 | `TX1 RX1` | "Telemetry 2" | Telemetry-stage candidate, not a Sensors-box port |
| B | UART2 | PD5/PD6 | `TX2 RX2` | "GPS1" | Real second GPS-capable port |
| C | UART3 | PD8/PD9 | `TX3 RX3` | "GPS2" | Today's default (#40) |
| D | UART4 | PB9/PB8 | `TX4 RX4` | "USER" | Free, generic candidate |
| E | UART6 | PC6/PC7 | `TX6 RX6` | 3 alternative modes (see below) | Input-stage (SBUS today) |
| F | UART7 | PE7/PE8 | `RX7 TX7` (product page also lists `RTS7`/`CTS7` for this UART at the chip level — not confirmed exposed on this board's own connector) | "Telemetry 1" | Telemetry-stage (S.Port today) |
| G | UART8 | PE1/PE0 | `TX8 RX8` | "USER" | Free, generic candidate |
| H | I2C1 | PB6/PB7 | `CL1 DA1` | "Compass, OLED" | Candidate for mag (#57) — matches INAV's own mag wiring on this board |
| I | I2C2 | PB10/PB11 | `CL2 DA2` (product page also notes this bus breaks out on a JST-GH-4P connector specifically, unlike the others) | "Onboard Barometer DPS310" | Onboard, matches `HELM_HAS_BARO`, not a port candidate |

Port E (UART6) is directly confirmed (manufacturer table) to have three
alternative single-protocol modes, not independently-combinable pin
assignments: `TX6 & RX6 → CRSF` (two-wire, one coherent full-duplex
config), `RX6 alone → SBUS/IBUS/DSM/PPM` (TX6 idle), `TX6 alone →
FPORT/SRXL2` (half-duplex single-pin). Plain S.Port is never listed as a
UART6 option — FPort is Matek's actual designed substitute (S.Port's own
telemetry payload, multiplexed onto one wire with SBUS-equivalent channel
data), not a different protocol. Deliberately out of scope for the core
routing architecture regardless ([ports.md](architecture/ports.md)'s own
scope note) — `matek_h743` keeps SBUS on Port E and S.Port on Port F, two
always-separate ports. Also surfaced but out of scope: `CAN1` (PD0/PD1).

"Suggested use" (GPS1/GPS2/USER/telem1/telem2) is Matek's documentation's
wording, not silkscreen — the physical pads are labeled `TX2`/`RX2` etc.

### afroflight32

Sourced from `aoa-boat-controller`'s `include/pins_naze32.h` (itself
sourced against Cleanflight's `target.c`/`target.h`/`hardware_revision.c`
and the Naze32 Rev6 owner's manual, per that file's own header comment) —
the same board family already backing this project's
`board.c`/`board_features.h`. Only 2 of this chip's 3 USARTs
are usable at all: USART3's default pins (PB10/PB11) collide with I2C2,
which this board's onboard IMU/baro bus already claims
(`board_i2c2_init()`) — the same reason Cleanflight's own NAZE target
never enables USART3 either. I2C1's pins are likewise fully claimed on
this board: both its default location (PB6/PB7) and its remap
alternative (PB8/PB9, per the STM32F103 reference manual RM0008's AFIO
remap table — a chip-level fact, not board-specific) land squarely on
this board's OUT3-OUT6 servo outputs (`pins_naze32.h`'s
`PIN_PWM_OUT_ROLL`/`PIN_PWM_OUT_S4`/`PIN_PWM_OUT_S5`/`PIN_PWM_OUT_S6` =
PB6/PB7/PB8/PB9). Neither USART3 nor I2C1 gets a port letter — no free
pins to route anything to.

USART1 (PA9/PA10) doesn't get a port letter either, same treatment —
it's the CLI's transport, over the onboard USB-serial converter, and
that's the only thing it will ever be. Confirmed by `aoa-boat-controller`'s
own investigation into reallocating this exact UART (`decisions.md`,
explored for S.Port, ultimately superseded): RX (PA10) is wired only to
the onboard USB-serial bridge chip and isn't broken out anywhere else on
the board, so no external peripheral's TX could ever be plugged in
regardless of what params say. The one accessory-header pad labeled
"UART1 TX" is the *same electrical net* as PA9 (`pinout.md`: "a second
physical access point onto the same already-claimed UART1 net, not an
independent third UART"), not a free alternate line — and owner-confirmed
against the physical board to be a bare pad, not a through-hole
connector, so it isn't practically wireable to anything even setting the
shared-net problem aside. Reassigning this UART away from CLI in
software would also cost the onboard USB connector *permanently* — PA9/
PA10 are hardwired to the converter chip with no disconnect jumper, so
there's no path back once it's given up. Flashing itself is unaffected
regardless (the ROM bootloader's AN3155 entry, #13/#28, is silicon-level,
not app-routing-dependent).

| Port | Peripheral | Pins | Silk label | Notes |
|---|---|---|---|---|
| A | USART2 | PA2/PA3 | `3`/`4` — this board's R/C input header is silkscreened with plain channel numbers 1-8, not function names (`pinout.md`'s board sketch: "8-pad INPUT column... numbered 1-8"); PA2/PA3 are confirmed pads 3/4 of that header, not separately function-labeled on the silk itself | Spare UART — input-stage (SBUS today, `HELM_HAS_SBUS_UART`); inversion is a separate GPIO (PB2, Cleanflight's target.h: "abused as inverter select"), not one of USART2's own pins |
| B | I2C2 | PB10/PB11 | `SDA`/`SCL` — the accessory header's own column, function-labeled unlike the numbered input header above (`pinout.md`'s board sketch: `SDA (PB11) \| SCL (PB10)`; the `(PBxx)` part is that doc's own annotation, `SDA`/`SCL` is the sourced silk text) | Carries the onboard IMU (MPU6500, 0x68) + baro (BMP280, 0x76) — matches `HELM_HAS_IMU`/`HELM_HAS_BARO`, no port claim needed for either (wired directly in `board_i2c2_init()`, same "never a real alternative" category `ports.md` already gives onboard chips). Unlike a UART port, though, this one *is* a real candidate on top of that: it's broken out to an actual through-hole header (GND/5V alongside SDA/SCL, confirmed against the board's own silkscreen/pinout diagram), and I2C is multi-drop by design — an external device (e.g. a mag, matching matek_h743's Port H framing) can share this exact bus at a different address with no config conflict, the way two protocols can never share one UART |

Also surfaced but out of scope, same treatment as `matek_h743`'s `CAN1`
callout: this board's S.Port telemetry runs on bit-banged software serial
(PB0/PB1, TIM3) rather than a UART peripheral at all, since both real
UARTs above are already spoken for — not a "Port" in this model's sense,
no hardware USART/I2C peripheral to route.

## Toolchain (decided)

PlatformIO, `framework = stm32cube` (raw HAL/LL, no Arduino) + FreeRTOS
(vendored, see [vendor/freertos-kernel](../vendor/freertos-kernel) —
Cortex-M3 and Cortex-M7 ports both vendored, since both are in active use
across the three targets). See repo root README's Status section for the
current milestone.

## Open items / not yet decided

- `afroflight32`'s battery-sense and GPS parity with `matek_h743` —
  no PDB-equivalent voltage/current sense documented yet, and no GPS
  header wired; tracked together as
  [#46](https://github.com/heimdall-suite/heimdall-helm/issues/46)
- `afroflight32`'s exact feature cuts — `board_features.h` has first-pass
  placeholder values marked TODO, not final decisions
- Real per-axis control-loop math (PID/attitude hold) — tracked as
  [#49](https://github.com/heimdall-suite/heimdall-helm/issues/49)
  (attitude estimation)/[#50](https://github.com/heimdall-suite/heimdall-helm/issues/50)
  (Pitch)/[#51](https://github.com/heimdall-suite/heimdall-helm/issues/51)
  (Roll, architecture TBD), not started — and CRSF decode/telemetry
  ([#9](https://github.com/heimdall-suite/heimdall-helm/issues/9)/[#19](https://github.com/heimdall-suite/heimdall-helm/issues/19)),
  also not started; see the feature matrix above and repo root README's
  Status section
- Exact numeric priority tiers for the module/scheduler architecture —
  the lifecycle/supervisor/fault-isolation model itself IS designed and
  bench-verified (see
  [.docs/architecture/module-architecture.md](architecture/module-architecture.md)),
  only the tier numbers remain open
- `nexus_xr`'s real HSE crystal value and pin map — needs real hardware or
  a real schematic, not more inference from spec sheets
- `nexus_xr` Port D (AUX/SBUS header, PB6/PB7): unlike Ports A-C,
  "unclaimed" here isn't idle — it's this board's own default servo
  outputs S8/S9. Whatever implements Port D's `.mode` field (#54) needs
  to agree with the output-mapping/servo-count system on which one wins
  when the port has no subsystem claim; not designed yet
- ST-Link/CubeProgrammer is not used for any target, by design — both
  real boards flash over their own ROM bootloaders instead (see
  [.docs/cli.md](cli.md)'s `dfu` command). Further real-hardware bring-up
  (`nexus_xr`, `afroflight32` battery/GPS/S.Port parity, blackbox
  storage) still needs to happen on the user's bench as each lands
