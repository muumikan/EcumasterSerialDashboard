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
| Clock | BM8563 RTC at 0x51, CR1220 cell diode-OR'd through a BAT54C |
| Storage | microSD on its own SPI bus, FAT16/FAT32 |
| ECU | Ecumaster EMU Classic, firmware 1.211 |
| Link | EDL-1 logger stream, 115200 baud, 8N1, one direction only |
| ECU port | EMU Classic extension port (replaces the BT Module) |

The dashboard replaces the Ecumaster BT Module on the EMU Classic's extension
port. That port speaks RS-232 voltage levels, which would destroy an ESP32
GPIO, so a MAX3232 sits between them — powered from the ECU at 3.3 V, never
5 V. See [wiring/signal-list.md](wiring/signal-list.md); the supply rule there
is the one that decides whether the board survives.

## What it shows

Six pages, switched by swiping horizontally. Only channels that have a sensor
actually fitted to the car appear anywhere; VSS and EGT are streamed by the
ECU but not wired, so they are not displayed.

| Page | Contents |
|---|---|
| **Drive** | RPM, boost with peak hold, CLT, oil pressure, lambda, battery |
| **Tune** | Lambda vs. target, knock, ignition advance, injector PW and duty, MAP, TPS, RPM |
| **Temps & press** | CLT, IAT, ECU temp, oil pressure, fuel pressure, ΔFPR |
| **Alarms** | Everything that tripped this run, newest first, with the time |
| **Diag** | Link state, frame age, counters, what latched, CEL word, run peaks |
| **Setup** | Alarm limits and behaviour, shift points, brightness, logging |

Above every page sit two always-on layers: a 6 px shift-light strip, lit from
1 000 rpm and red from 6 000, and a status bar carrying the link state, the
worst active alarm and the time of day regardless of which page is up. The car
has no clock in its own instrument cluster, so this is the only one.

The page changes only when you swipe it. There is no idle timeout returning to
Drive: on the car it moved the screen out from under you while you were still
reading it.

An out-of-range value lights its own cell — amber for a warning, red for
critical — and names itself in the status bar. Nothing but a swipe ever changes
the page. Every trip is recorded with its time, the value, the RPM it happened
at and how long it lasted, so a half-second dip is still there when you stop.

The Alarms page lists those records newest first. Limit crossings, the ECU's
check-engine bits and the health of the serial link all land in the same list,
because a loose connector and a hot engine are both things you want to see in
the order they happened. Nothing is acknowledged and nothing is cleared by
hand: it is a log to read, not a queue to work through.

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

## The serial link

The dashboard reads the EMU's **EDL-1** logger stream: 260-byte frames at
115200 baud carrying all 195 channels from one instant, rather than the older
protocol's 34 channels arriving one 5-byte frame at a time. The ECU has to be
configured to send it.

The classic protocol was supported too, chosen by a build flag, until it was
removed — it could not produce a `.emulog` at all, and was compiled on every
change without ever being run. See the
[decision log](docs/decision-log.md). The adapter seam it justified is still
there, so adding a protocol back costs nothing above that line.

The channels are listed in [docs/edl-channels.md](docs/edl-channels.md); most
are not on screen yet.

## Service page

With the engine stopped, the dashboard raises a WiFi access point and serves a
maintenance page on it: download or delete the logs, edit the same settings the
panel holds, set the clock from the browser, and read the diagnostics that
otherwise need a USB cable behind the dash.

Join `EcuDash` and open `http://192.168.4.1/`. It is off until switched on
under `Log` on the setup page. Full details, including why the engine has to be
stopped, are in [docs/service-page.md](docs/service-page.md).

## Building

The project uses [PlatformIO](https://platformio.org/).

```bash
pio run                     # build
pio run -t upload           # build and flash over USB-C
pio device monitor          # 115200 baud USB CDC console; press any key for the state
```

There is one environment, `crowpanel_advance_35`, so no `-e` is needed. If
`pio` is not on your `PATH`, it lives at `~/.platformio/penv/bin/pio`.

Dependencies (LovyanGFX and LVGL 8.3) are pulled automatically. The EDL-1 field
extraction is vendored in `lib/EDLSerial/` — it is
[ThiloZ/EDLSerial](https://github.com/ThiloZ/EDLSerial) (MIT), with the framing
done in this project instead; see
[docs/architecture.md](docs/architecture.md) for why.

## Layout

```
include/            headers, one per module
src/protocol/       EdlSerialAdapter — the only place that knows the wire format
src/data_model/     EngineDataModel — the single source of truth for the UI
src/ecu/            EcuDataProvider — owns UART1
src/alarms/         AlarmEngine — thresholds in one rule table, and the event log
src/screens/        LVGL pages, tiles, chrome and navigation
src/diagnostics/    diagnostics page and the serial text report
src/logging/        EmuLog — .emulog files to the SD card
src/hal/            display, touch and RTC bring-up
src/web/            service access point, HTTP server and the page it serves
docs/               architecture, protocol, decisions
hardware/ wiring/   pin assignment and the ECU-to-dash signal chain
```

See [docs/architecture.md](docs/architecture.md) for how the layers fit
together and [docs/decision-log.md](docs/decision-log.md) for why they are
shaped that way.

## Status

**Runs on the car.** Tested connected to the ECU on 8 and 9 September 2026. On
EDL-1 the link runs clean with no dropped frames, values read correctly and the
alarm list behaves. See [docs/test-results.md](docs/test-results.md).

Working: the EDL-1 link, data model, alarm engine, eight pages, swipe
navigation, settings in flash, the real-time clock, SD logging, and the service
access point with its page — access point, page, log download and clock sync
confirmed on the car on 16 September 2026.

Not yet exercised on the car: the multi-file `.tar` download, deleting logs,
starting the engine mid-download, and the idle timeout. They are listed with
what to look for in [docs/test-results.md](docs/test-results.md).

One thing to know about the parts that work. The SD card has mounted intermittently
on the bench for reasons nobody has established; `EmuLog` retries and says
which attempt worked, so the console tells you if it happens again.

Open items are listed in [docs/decision-log.md](docs/decision-log.md).
