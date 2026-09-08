# Ecumaster EMU Classic serial dashboard

A digital dashboard for a Toyota Carina with a 1G-GTE running an Ecumaster
EMU Classic. An ESP32-S3 reads the ECU's serial data stream and renders it on
a 3.5" touch panel.

**The link is read-only.** Nothing in the firmware writes to the ECU port,
and the ECU's extension port is a one-way stream in the first place.

## Hardware

| Part | Detail |
|---|---|
| Display / MCU | Elecrow CrowPanel Advance 3.5" HMI, ESP32-S3-WROOM-1-N16R8 |
| Panel | ILI9488, 480 × 320 IPS, GT911 capacitive touch |
| Memory | 16 MB flash, 8 MB octal PSRAM |
| Level shifter | MAX3232 (RS-232 ↔ 3.3 V TTL) |
| ECU | Ecumaster EMU Classic, firmware 1.211 |
| Link | EDL-1 logger stream, 115200 baud, 8N1, one direction only |
| ECU port | EMU Classic extension port (replaces the BT Module) |

The dashboard replaces the Ecumaster BT Module on the EMU Classic's extension
port. That port speaks RS-232 voltage levels, which would destroy an ESP32
GPIO, so a MAX3232 sits between them — powered from the ECU at 3.3 V, never
5 V. See [wiring/signal-list.md](wiring/signal-list.md); the supply rule there
is the one that decides whether the board survives.

## What it shows

Five pages, switched by swiping horizontally. Only channels that have a sensor
actually fitted to the car appear anywhere; VSS and EGT are streamed by the
ECU but not wired, so they are not displayed.

| Page | Contents |
|---|---|
| **Drive** | RPM, boost with peak hold, CLT, oil pressure, lambda, battery |
| **Tune** | Lambda vs. target, knock, ignition advance, injector PW and duty, MAP, TPS, RPM |
| **Temps & press** | CLT, IAT, ECU temp, oil pressure, fuel pressure, ΔFPR |
| **Diag** | Link state, frame age, counters, what latched, CEL word, run peaks |
| **Setup** | Alarm limits and behaviour, shift points, brightness, logging |

Above every page sit two always-on layers: a 6 px shift-light strip, lit from
1 000 rpm and red from 6 000, and a status bar carrying the link state and the
worst active alarm regardless of which page is up.

The page changes only when you swipe it. There is no idle timeout returning to
Drive: on the car it moved the screen out from under you while you were still
reading it.

An out-of-range value lights its own cell — amber for a warning, red for
critical — and names itself in the status bar. Nothing but a swipe ever changes
the page. Every trip is latched with the value and the RPM it happened at, so a
half-second dip is still there when you stop.

Alarms arm only after the engine has been running for a few seconds. Cranking
crosses 500 rpm while oil pressure is still building and the battery is still
down from the starter, and without the delay every start would fire two
critical alarms. Thresholds carry a deadband so a value sitting on its limit
does not flicker.

Limits, the arming delay, the deadband, the shift points and the backlight are
all editable on the Setup page and stored in flash — no laptop, no reflash. The
page stays usable with the engine running, because setting a limit without
watching the value it guards is guesswork.

At power-up the dash opens on Drive and sweeps the shift lights, so every
segment is confirmed working before the car moves.

## Two protocols

The EMU can stream either of two serial protocols, and the dashboard reads
both. Which one is built is a compile-time choice, because they differ in baud
rate and framing and the ECU is configured for one or the other.

| | Classic | **EDL-1** |
|---|---|---|
| Environment | `crowpanel_advance_35` | `crowpanel_advance_35_edl` |
| Baud | 19200 | 115200 |
| Frame | 5 bytes, one channel | 260 bytes, every channel |
| Channels | 34 | **195** |
| Samples | channels arrive apart | all from one instant |

EDL-1 is what the car runs and what a bare `pio run` builds. Everything above
the adapter — model, alarms, screens, settings — is identical either way.

The extra channels are listed in [docs/edl-channels.md](docs/edl-channels.md);
most are not on screen yet.

## Building

The project uses [PlatformIO](https://platformio.org/).

```bash
pio run                                          # build the default (EDL-1)
pio run -t upload                                # build and flash over USB-C
pio run -e crowpanel_advance_35 -t upload        # the classic protocol instead
pio device monitor                               # 115200 baud USB CDC console
```

If `pio` is not on your `PATH`, it lives at `~/.platformio/penv/bin/pio`.

Dependencies (LovyanGFX and LVGL 8.3) are pulled automatically. The EMU
protocol decoder is vendored in `lib/EMUSerial-master/` — it is
[GTO2013/EMUSerial](https://github.com/GTO2013/EMUSerial), used unmodified as
the protocol reference.

## Layout

```
include/            headers, one per module
src/protocol/       EMUSerial adapter — the only place that knows the wire format
src/data_model/     EngineDataModel — the single source of truth for the UI
src/ecu/            EcuDataProvider — owns UART1
src/alarms/         AlarmEngine — thresholds in one rule table
src/screens/        LVGL pages, tiles, chrome and navigation
src/diagnostics/    diagnostics page and the serial text report
src/hal/            display and touch bring-up
docs/               architecture, protocol, decisions
hardware/ wiring/   pin assignment and the ECU-to-dash signal chain
```

See [docs/architecture.md](docs/architecture.md) for how the layers fit
together and [docs/decision-log.md](docs/decision-log.md) for why they are
shaped that way.

## Status

**Runs on the car.** Tested connected to the ECU on 8 September 2026, on both
protocols. On EDL-1 the link runs clean with no dropped frames. See
[docs/test-results.md](docs/test-results.md).

Working: both serial protocols, data model, alarm engine, five pages, swipe
navigation, settings in flash. SD logging exists on the `feature/sd-logging`
branch; it writes a file, but that file does not open in EMU Classic Client —
see [docs/emu-log-format.md](docs/emu-log-format.md) on that branch.

Open items are listed in [docs/decision-log.md](docs/decision-log.md).
