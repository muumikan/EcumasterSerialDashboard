# Decision log

Why the project is shaped the way it is. Newest first.

---

## Alarms colour the cell; only critical ones move the page

An out-of-range value lights its own tile — amber for a warning, red for
critical — and the status bar names the worst one on every page. A full-screen
takeover was built first and then dropped: it hides everything else at exactly
the moment the driver most wants context.

Page switching is narrower still. A **critical** alarm requests the page that
shows it, once, on its rising edge; the request is then consumed. A warning
never moves the page.

The reason is the failure mode of the obvious alternative. A dash that jumps
whenever a condition is true fights the driver: coolant sitting at 101 °C up a
long hill would drag the screen back every time they swiped away. Rising-edge
plus consume means the dash speaks once and then respects the choice, and only
speaks again if the condition clears and returns.

## Alarms are gated on RPM > 500

With the key on and the engine stopped, oil pressure reads 0 bar and battery
voltage sits near 12.4 V. Both would be critical by any sensible threshold,
and neither is a fault.

Gating on the engine actually turning is simpler and more honest than
special-casing each rule, and it makes the intent obvious in one constant:
`kEngineRunningRpm`.

## CEL bits are shown as numbers, not names

The 16-bit `cel` word is decoded by the reference implementation, but the
meaning of each bit is not documented there. A mockup of the diagnostics page
carried plausible labels — CLT, IAT, TPS and so on — and those were invented.

The page now shows the raw hex word and 16 numbered bit indicators. Names can
be filled in once they are confirmed against the EMU software, and not before.

## Unwired channels appear nowhere

The ECU streams all 35 channels whether or not a sensor exists. Reading the
car's schematic showed that VSS, both EGT inputs, oil temperature, fuel level,
flex-fuel content and gear position have nothing behind them.

Displaying a confident `0` for a channel with no sensor is worse than
displaying nothing, so those channels are absent from every page. This is a
per-car decision, which is why it is recorded here rather than inferred from
the code.

## The ECU link has no TX pin at all

`board::kEcuTxPin` is `-1`, so `HardwareSerial::begin()` never attaches a
transmit pin to UART1.

The project rule is that communication is read-only. Enforcing it only in
software would leave one careless `write()` between the dashboard and a
running engine's ECU. With no pin attached, that call goes nowhere.

## ECU RX moved from GPIO16 to GPIO18

Bring-up ran on a generic esp32dev board with the ECU on GPIO16. On the
CrowPanel Advance 3.5, **GPIO16 is the GT911 touch I2C clock**.

The ECU input moved to GPIO18, the RX pin of the board's UART1-OUT connector.
This requires a physical change to the loom, not just a rebuild.

## Target moved fully to the CrowPanel; esp32dev dropped

The esp32dev environment was a protocol bring-up rig. Once the CrowPanel was
on the bench there was no reason to carry a second PlatformIO environment, so
it was removed rather than kept "just in case".

## Pin numbers are transcribed, never guessed

Every GPIO in `board_config.hpp` carries a source comment pointing at
Elecrow's wiki or their demo repository. The same standard the project applies
to the ECU protocol — *do not invent protocol details* — applies to hardware:
a wrong pin number costs a board, and a plausible guess is indistinguishable
from a fact once it is written down.

## The protocol decoder is vendored, not reimplemented

`lib/EMUSerial-master/` is GTO2013's library, unmodified. The adapter above it
translates its struct into the project's own type but re-scales nothing,
because the dividers in the format file *are* the protocol.

Writing a cleaner decoder was tempting and was not done. The reference
implementation is the specification available.

---

# Open items

- **Which Analog In carries oil pressure and which carries fuel pressure.**
  Not readable from the schematic — the wire routing has no text. Harmless for
  the firmware provided both channels are assigned in the EMU software; if
  they are not, the values arrive only as raw volts on `analogIn1..4`.
- **The EMU terminal that carries serial TX** is not recorded in
  `wiring/signal-list.md` yet.
- **A Bluetooth module appears in schematic rev16.** If it shares the serial
  port, decide whether the port drives both or the dash replaces it.
- **Not yet run against the car.** The firmware compiles and the layout is
  fixed at 480 × 320, but nothing has been verified on the bench. Two things
  to watch on the first flash: whether the LVGL object pool is large enough
  (`LV_MEM_SIZE`, currently 96 kB), and whether swipe gestures bubble
  correctly when the touch lands on a tile rather than the background.
- **Fonts.** LVGL's built-in Montserrat is not condensed and stops at 48 px,
  so the RPM readout is smaller than the mockup's. `λ` and `Δ` are outside its
  character set, so the UI uses `LAMBDA`, `DFPR` and `C` instead of `°C`. A
  custom font subset would fix all of this at once.
- **Alarm state freezes when the link drops.** Alarms are only re-evaluated
  when the model's revision changes, so the last known state persists while
  the status bar reads `OFFLINE`. Deliberate for now; clearing them on
  `Offline` is a small change in `DashUi::update`.
