# Decision log

Why the project is shaped the way it is. Newest first.

---

## Alarms colour the cell and name themselves; they never move the page

An out-of-range value lights its own tile — amber for a warning, red for
critical — and the status bar names the worst one. The status bar is on screen
whichever page is up, so the driver already sees it.

Two more aggressive designs were built and then removed. A full-screen takeover
hid everything else at exactly the moment the driver most wants context. Pulling
the page that owns the value was gentler but still bought nothing the status bar
was not already showing, while taking the screen away from whatever the driver
had deliberately chosen to look at.

What remains is the quiet version: colour where the value lives, one line of
text that follows you across pages, and the page changes only when a thumb
changes it.

## The EDL-1 protocol replaced the classic one

The EMU streams either its classic serial protocol or the EDL-1 logger
protocol. The classic one sends 34 channels, one per 5-byte frame at 19200,
which means values arrive at different moments and have to be assembled into
something that never existed as a single sample. EDL-1 sends 260 bytes at
115200 carrying all 195 channels from one instant.

Three reasons, any one of which would have been enough: nearly six times the
channels, a genuine simultaneous sample rather than a rolling reconstruction,
and - the one that matters for logging - the same stream an EDL-1 datalogger
receives before writing the `.bgc` files EMU Classic Client opens.

Both live in the tree behind one interface, chosen at build time by
`ecu_link.hpp`, because they differ in baud and framing and the ECU is
configured for one or the other. The default build is EDL-1, since that is
what the car is set up for; building classic against an EDL-1 ECU produces a
dashboard that shows nothing.

Adopting it cost no change above the adapter layer. That was the point of
having one.

## Framing has to do the work a checksum would

The EDL-1 protocol carries no checksum. On the first drive that showed as
bursts of impossible values across several fields at once - a coolant maximum
of 27472 C, oil pressure at 15.9 bar, invented check-engine flags.

Several fields wrong at the same moment is the signature of a misaligned
frame, not a faulty sensor, and the cause was mundane: Arduino's default UART
receive buffer is 256 bytes, smaller than a single 260-byte frame and about
22 ms of slack at 115200. Any redraw or flash write that held the loop longer
dropped bytes, and a dropped byte splices two frames into one that still opens
with a valid marker.

The buffer is now 2048 bytes, and three checks stand behind it because the
loop will stall again eventually: a one-byte sliding window rather than the
vendored library's discard-everything approach, which preserves a phase error
forever; a refusal to accept a frame until the next marker lands exactly 260
bytes later, which is the thing a splice cannot fake; and a plausibility gate
using Ecumaster's own `maxLimit` values.

The last piece matters most in practice. The adapter keeps the last *good*
frame rather than the last frame. One bad sample used to be permanent, because
it went straight into the run peaks and stayed there for the rest of the drive.

## The page changes only when a thumb changes it

Two mechanisms used to move it on their own, and both are gone.

A critical alarm used to pull up the page showing the value. It bought nothing
the status bar was not already saying, and it took the screen away from
whatever the driver had chosen.

An idle timeout used to return to Drive after thirty seconds. On the car that
turned out to be worse: it moved the screen out from under you while you were
still reading a page you had deliberately opened. Manual is the whole rule now.

## A run ends when the data stops, not when the revs do

The engine-off summary keyed on RPM falling below the running threshold, and
on the car it never appeared.

Killing the ignition cuts the ECU's power as well, so the last frame it ever
sends freezes at whatever the engine was doing - often several hundred rpm -
and the model faithfully keeps reporting it. Waiting for that number to fall
waits for ever.

The summary now closes the run when the engine stops *or* the link goes
offline, and it is checked on every pass rather than only when a frame arrives.
That second half matters as much as the first: the situation it exists for is
precisely the one where frames have stopped.

## Settings live in flash, and only numbers live there

The setup page edits an `AlarmSettings`/`DashSettings` struct saved to NVS as
one versioned blob. The version is bumped whenever the struct changes shape, so
an update that removes a field comes up on defaults rather than reading the old
bytes as new ones. Which value an alarm watches and which way it trips stays
in the rule table in code.

That line matters. A settings page that can rewire logic is a settings page
that can brick the dash on a dark road; one that can only move numbers cannot
produce a state the code has not already been written to handle. The stored
record carries a magic word and a version, and a mismatch falls back to
defaults rather than reinterpreting old bytes as new fields.

The page stays editable with the engine running. Locking it while stopped was
the first design and it was wrong: setting an oil pressure limit without
watching the live value it guards is exactly the guesswork the page exists to
end.

## Alarms arm on a delay, not on RPM alone

Gating on RPM > 500 alone fired on every start. Cranking crosses 500 rpm while
oil pressure is still building and the battery is still down from the starter,
so two critical alarms went off every time the engine caught - the fastest
possible way to teach a driver that red means nothing.

The engine now has to have been running for the arming delay before anything
can trip, and the status bar counts it down rather than going silently quiet.
The default started at three seconds and was raised to six after watching a
real start: oil pressure had not finished building by three.

## Thresholds carry a deadband, and trips latch

A value resting on its limit flickered its cell at the frame rate. An alarm now
trips at the limit and clears only once the value has moved back past it by the
hysteresis share.

Latching matters more. A half-second oil pressure dip in a corner is the event
most worth knowing about and the one a live-only display loses completely. Each
alarm keeps its worst moment - value and RPM - until the run ends.

## Alarms are gated on RPM > 500

With the key on and the engine stopped, oil pressure reads 0 bar and battery
voltage sits near 12.4 V. Both would be critical by any sensible threshold,
and neither is a fault.

Gating on the engine actually turning is simpler and more honest than
special-casing each rule, and it makes the intent obvious in one constant:
`kEngineRunningRpm`.

## CEL bits are named, once a source for the names existed

The 16-bit `cel` word is decoded by the reference implementation, but the
meaning of each bit is not documented there. An early mockup carried plausible
labels — CLT, IAT, TPS and so on — and those were invented, so the page shipped
with bare numbered bits instead.

The names turned up later in Ecumaster's own format definition, in the 1.211
XML that came off a USB stick, as `<paramlist name="checkEngine">`. Five of the
invented labels happened to be right and three were not, which is roughly what
guessing is worth.

The page now names the eleven defined bits. One inference remains — that the
list's entry N is bit N-1 — and it is written down in
[ecu-protocol.md](ecu-protocol.md#check-engine-bits) with the field-width
argument for it and a way to check it on the car.

## Unwired channels appear nowhere

The ECU streams all 35 channels whether or not a sensor exists. Reading the
car's schematic showed that VSS, both EGT inputs, oil temperature, fuel level,
flex-fuel content and gear position have nothing behind them.

Displaying a confident `0` for a channel with no sensor is worse than
displaying nothing, so those channels are absent from every page. This is a
per-car decision, which is why it is recorded here rather than inferred from
the code.

## The dashboard replaces the Ecumaster BT Module

The car's schematic showed a Bluetooth module on the EMU Classic's extension
port. The dashboard takes that place rather than sharing the port, which
settles what it plugs into and where its level shifter is powered from.

The BT Module manual then decides the electrical detail: that port supplies
**3.3 V**, it also exposes a +5 V pin that "must not be used", and 5 V "may
result in permanent damage". The MAX3232 is therefore powered from the ECU at
3.3 V, and the panel's own 3.3 V rail is not bridged to it — only grounds are
common. See [../wiring/signal-list.md](../wiring/signal-list.md).

The manual also confirms the stream's shape: *"Data transmission: One-way
(ECU → external device)"*.

## The ECU TX line is wired, and read-only is enforced in software

This reverses an earlier decision. `board::kEcuTxPin` was `-1` so that
`HardwareSerial::begin()` never attached a transmit pin, making a stray
`write()` physically harmless.

It is now GPIO17, because that is the wiring and the configuration the link
was actually verified working with. Changing the pinout at the same time as
everything else would have meant debugging two things at once if the first
flash had stayed silent.

The read-only guarantee is therefore a software one: the provider and the
adapter are the only files that touch the ECU port, and neither ever calls
`write()` or any other transmitting method. The port being one-way by design
means nothing is listening anyway.

If the hardware guarantee is wanted back, setting `kEcuTxPin` to `-1` restores
it in one line — the pin is assigned but never driven either way.

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
- **Extension port pin numbering.** The BT Module manual gives the supply
  rule in text but the pin numbers only in a figure, which could not be read
  out of the PDF. Pin 5 is +5 V and must not be used; the 3.3 V, ground and
  serial pins still have to be read off the manual's drawing and recorded in
  `wiring/signal-list.md` before anything is soldered.
- **The ECUMASTER serial protocol must be enabled** in the EMU Classic Client
  and made permanent, or the port stays quiet.
- **The 1.200/1.211 channel 33 mismatch is moot on EDL-1**, which carries
  `afrTarget` and `scondarypulseWidth` as separate fields at fixed offsets. It
  still applies to the classic environment; see
  [ecu-protocol.md](ecu-protocol.md#version-mismatch-channel-33).
- **The check-engine bit numbering is an inference.** Confirm it by unplugging
  the intake air sensor and checking that IAT is the flag that lights.
- **160 EDL-1 channels are decoded but not displayed.** Listed in
  [edl-channels.md](edl-channels.md); boost control, knock-versus-noise,
  trigger health and idle control are the ones this car can actually use.
- **SD logging is untested.** It lives on the `feature/sd-logging` branch and
  has never been run. The format question in
  [emu-log-format.md](emu-log-format.md) is still open too.
- **UI repaints at frame rate.** `DashUi` updates the visible page whenever the
  model's revision changes, which at 19200 baud is a few hundred times a
  second. LVGL coalesces the redraw, but the formatting work is done every
  time. Rate-limiting to about 20 Hz would cut it by an order of magnitude and
  make the digits readable rather than a blur.
- **LVGL object pool headroom is unmeasured.** `LV_MEM_SIZE` is 96 kB. The
  Setup page's Limits category builds the most objects of any screen; if it
  ever comes up blank or the dash restarts on the way to it, that is the first
  thing to raise.
- **Enclosure dimensions are not verified.** `enclosure/case.scad` is
  parametric and its geometry is right, but the measurements at the top of the
  file are placeholders. They must be taken from Elecrow's STEP model or the
  physical panel before printing.
- **Fonts.** LVGL's built-in Montserrat is not condensed and stops at 48 px,
  so the RPM readout is smaller than the mockup's. `λ` and `Δ` are outside its
  character set, so the UI uses `LAMBDA`, `DFPR` and `C` instead of `°C`. A
  custom font subset would fix all of this at once.
- **Alarm state freezes when the link drops.** Alarms are only re-evaluated
  when the model's revision changes, so the last known state persists while
  the status bar reads `OFFLINE`. Deliberate for now; clearing them on
  `Offline` is a small change in `DashUi::update`.
