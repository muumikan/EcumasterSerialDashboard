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
| Link | 19200 baud, 8N1, one direction only |
| ECU port | EMU Classic extension port (replaces the BT Module) |

The dashboard replaces the Ecumaster BT Module on the EMU Classic's extension
port. That port speaks RS-232 voltage levels, which would destroy an ESP32
GPIO, so a MAX3232 sits between them — powered from the ECU at 3.3 V, never
5 V. See [wiring/signal-list.md](wiring/signal-list.md); the supply rule there
is the one that decides whether the board survives.

## What it shows

Four pages, switched by swiping horizontally. Only channels that have a sensor
actually fitted to the car appear anywhere; VSS and EGT are streamed by the
ECU but not wired, so they are not displayed.

| Page | Contents |
|---|---|
| **Drive** | RPM, boost with peak hold, CLT, oil pressure, lambda, battery |
| **Tune** | Lambda vs. target, knock, ignition advance, injector PW and duty, MAP, TPS, RPM |
| **Temps & press** | CLT, IAT, ECU temp, oil pressure, fuel pressure, ΔFPR |
| **Diag** | Link state, frame age, update and revision counters, CEL word, run peaks |

Above every page sit two always-on layers: a 6 px shift-light strip, lit from
3 500 rpm and red from 6 800, and a status bar carrying the link state and the
worst active alarm regardless of which page is up.

An out-of-range value lights its own cell — amber for a warning, red for
critical — rather than taking over the screen. A critical alarm also brings up
the page that shows it, once, so swiping away is respected. Alarms are gated
on RPM > 500: with the key on and the engine stopped, oil pressure reads 0 bar
and battery voltage sits near 12.4 V, and neither is a fault.

## Building

The project uses [PlatformIO](https://platformio.org/). There is one
environment, `crowpanel_advance_35`.

```bash
pio run                 # build
pio run -t upload       # build and flash over USB-C
pio device monitor      # 115200 baud USB CDC console
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

Working: serial link, data model, alarm engine, all four pages, swipe
navigation. Built and verified to compile; not yet run against the car.

Open items are listed in [docs/decision-log.md](docs/decision-log.md).
