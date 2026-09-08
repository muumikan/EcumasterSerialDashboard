# Architecture

The rule the code is written against: **the UI never learns the wire format,
and the protocol layer never learns about timing or pixels.** Each layer below
only knows the one under it.

```
        LVGL screens              DashUi, MainScreen, TuneScreen, TempsScreen,
              │                   DiagnosticScreen, SetupScreen, Tile
              │
              │   DashSettings ◀──▶ SettingsStore (NVS)
              │        │            numbers and flags only
              ▼        ▼
        AlarmEngine ──────────┐   one rule table, limits from settings
              │               │   arming delay, hysteresis, latching
              ▼               ▼
        EngineDataModel ──────┘   snapshot in engineering units,
              │                   link health, revision counter
              ▼
        EcuDataProvider           owns UART1, stamps arrival times
              ▼
        EcuLinkAdapter            EmuSerialAdapter (classic, 34 channels)
                                  EdlSerialAdapter (EDL-1, 195 channels)
              ▼                   chosen at build time by ecu_link.hpp
        UART1  ◀── MAX3232 ◀── EMU Classic
```

## The layers

### Adapters — `src/protocol/`

Two, for the EMU's two serial protocols, behind one interface. `ecu_link.hpp`
picks between them at build time and supplies the matching baud rate; nothing
above this layer knows which is in use. This is the seam the layering existed
for, and swapping protocols cost no change to the model, the alarms, the
screens or the settings.

#### EdlSerialAdapter

The EDL-1 logger stream: 260-byte frames at 115200 carrying all 195 channels
from one instant. Field extraction comes from the vendored
[ThiloZ/EDLSerial](https://github.com/ThiloZ/EDLSerial) (MIT); framing is done
here, because the protocol carries no checksum and the library's own
resynchronisation is wrong.

Three things earn the confidence a checksum would otherwise give: a one-byte
sliding window to find the marker, a refusal to accept a frame until the next
marker appears exactly 260 bytes later, and a plausibility check that rejects
values which cannot be real. The adapter keeps the last *good* frame rather
than the last frame, so a bad sample never reaches the run peaks.

#### EmuSerialAdapter

Wraps [GTO2013/EMUSerial](https://github.com/GTO2013/EMUSerial) unmodified and
translates its `emu_data_t` into `EngineSnapshot`. Nothing is re-scaled here:
the decoder already produces engineering units, and inventing a conversion
would mean inventing protocol. This is the only file in the project that
includes `EMUSerial.h`.

It reports bytes consumed rather than frames decoded, because
`checkEmuSerial()` returns nothing and `decodeEmuFrame` is private. That byte
count is what liveness is derived from — see the caveat in
[decision-log.md](decision-log.md). Its bad-frame count is always zero for the
same reason: the decoder surfaces no failures to count.

### EngineDataModel — `src/data_model/`

The single source of truth. Holds the latest `EngineSnapshot`, when it
arrived, and a `revision()` counter that advances **only when a value actually
changed**, so the UI can skip redraws with one integer comparison.

Link health is derived from arrival timing alone: `Online`, then `Stale` after
500 ms, then `Offline` after 3 s.

### EcuDataProvider — `src/ecu/`

Owns `HardwareSerial(1)`, opens it with no TX pin, drains it, and stamps the
model. The seam between hardware and application state.

### AlarmEngine — `src/alarms/`

Evaluates the snapshot against a fixed rule table: each rule gives a severity,
a status-bar string, and the page that displays the value. Sitting beside the
model rather than inside the screens means a threshold is changed in one place
and all five pages agree.

Three behaviours are deliberate. Alarms arm only after the engine has been
running for the arming delay, so cranking cannot trip them. Thresholds carry a
deadband so a value on its limit does not flicker. And every trip is latched
with its worst value and the RPM it happened at, because a half-second dip is
exactly what a live-only display loses.

No alarm ever changes the page.

### Screens — `src/screens/`, `src/diagnostics/`

`DashUi` owns everything that is always on screen — the shift-light strip, the
status bar, the page dots — plus navigation: swipe gestures, the critical-alarm
page request, and the 30-second return to the driving page.

Each page implements `DashPage`. Pages receive the model, the alarm state and
the run peaks; they hold no thresholds and no protocol knowledge. `Tile` is
the shared measurement cell and owns its own severity colouring.

Only the visible page is updated, and only when the model's revision moves.

### Settings — `src/settings/`

`DashSettings` holds everything the driver can change: alarm limits and
enables, the arming delay and deadband, shift points, backlight and logging. `SettingsStore` keeps it in NVS as one versioned blob, so a
firmware change that alters the struct falls back to defaults rather than
reading old bytes as new fields.

Only numbers and flags live there. Which value an alarm watches and which way
it trips stays in the rule table in code.

### Display — `src/hal/`

LovyanGFX panel and GT911 touch configuration, and LVGL bring-up. Keeps the
panel wiring out of the screen code. Draws through two partial buffers in
internal DMA-capable RAM rather than full-screen buffers in PSRAM, which
flushes faster.

## Data flow, one iteration

```
AppController::loop()
  provider.loop(now)      drain UART → decode → model.applySnapshot()
  ui.update(model, now)   if revision moved: evaluate alarms, record peaks,
                          repaint shift lights, status bar, visible page
  display::loop()         lv_timer_handler()
  report.update(...)      text diagnostics on the USB console
```

`main.cpp` is two lines. Everything else hangs off `AppController`.
