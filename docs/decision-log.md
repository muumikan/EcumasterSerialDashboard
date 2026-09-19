# Decision log

Why the project is shaped the way it is. Newest first.

---

## A measurement's name is yellow, and its unit is on the caption's line

Two changes that came out of reading the panel in the car rather than on a
desk, and they are the same change.

The name of a measurement was grey on black, which is the lowest contrast
anywhere on this dashboard, on the one element that tells you what you are
looking at. It is yellow now. That is also what an Ecumaster dash looks like,
so the panel matches the software it sits beside; a driver who knows one reads
the other without relearning it.

The yellow is `#E8C547` and is deliberately clear of the warning amber
`#E8A33D`, which has red in it. The two are never far apart on screen and must
not be confused: one is a name, the other is a verdict.

The unit moved from the bottom right of the cell to the top right, beside the
name. Bottom right put it on the value's own baseline, which meant every
increase in the value's size walked the two closer together - and the values
did need to grow. It also reads better: what this is and what it is measured
in are the same kind of fact, and the line below is then the value's alone.

Values on the three-across pages went from 28 px to 36 px, which is the
largest LVGL's built-in Montserrat offers that still fits a 160 x 95 cell
under its caption. 48 does not: its line box is 52 px against 79 px of cell
inside the padding, which leaves nothing for the caption. The one cell that
stayed smaller is IDLE CTL, because it holds a word rather than a number, and
"CLOSED" is 154 px at 36 - wider than the cell.

---

## The status bar is slots, not offsets

Everything on the status bar now sits in a slot given as a fixed left edge and
a fixed width, and clips to it. It used to be placed by eye: each item at an
offset from the right edge, and the alarm line simply centred.

Those two arrangements did not know about each other. Park the car with the
access point up and "ENGINE OFF" grew leftwards from the centre underneath
"WIFI", which is the one situation where both are shown - so the bug was
invisible on the bench and unavoidable in the driveway.

The widths are measured rather than guessed, by reading the advance widths out
of `lv_font_montserrat_14.c` for the longest string each slot can hold. That
turned up a second latent bug: "OFFLINE" is 63 px, and the slot that looked
about right was 62.

The access point indicator became LVGL's wifi glyph instead of the word. It
says the same thing in a third of the width, on a bar that had already run out
of room once.

---

## Target and actual share one absolute scale

TUNE draws mixture as a deviation bar: AFR minus target, centred, ±1.8 across
the width. IDLE and BOOST were specified to work "like the AFR bar", and they
do not. Both draw the actual value as a fill on an absolute scale with the
target as a mark on that same scale.

A deviation bar answers "how far off are we", which is only a question while
the loop is closed and aiming at something. Mixture always has a target. Idle
does not: drive away and idle control opens, the target stops meaning
anything, and a deviation bar sits pegged for the rest of the session looking
exactly like a fault. Boost is worse - most of any drive is spent nowhere near
target, by intent.

An absolute bar degrades into something still true: a rev counter, and a
pressure gauge. The target mark simply disappears when the ECU is not asking
for one, which it signals by sending zero.

The scales are 0–2000 rpm and 0–220 kPa. Both are chosen so the interesting
region sits in the middle of the bar rather than at one end: idle targets run
850–1500, and 220 kPa puts atmospheric just under halfway, so off boost is a
position the eye recognises instead of a number it has to read. Driving pegs
the idle bar, which is the honest behaviour for a scale that stops at 2000.

BOOST shows absolute kPa, while DRIVE shows gauge bar. That is deliberate:
`boostTarget` is stated in absolute kPa by the ECU, and converting one half of
a comparison puts arithmetic between the driver and the thing being compared.
DRIVE is for driving and bar is what a driver thinks in; BOOST is for setting
up the loop and kPa is what the loop is configured in.

## Absence is a value, and a zero cannot say it

The classic 35-channel protocol carries no idle or boost channel at all, so on
that build both new pages have nothing to show. Nothing distinguishes that from
a working loop: a closed idle valve, a settled PID and a missing channel all
decode to 0.

So `EngineSnapshot` carries a `controlChannels` flag, set by the adapter that
filled the snapshot, and the pages print `n/a` when it is false. Two
alternatives were rejected. An `#ifdef` in the page would put protocol
knowledge back into the UI, which is the one thing this project's layering
exists to prevent. Leaving the zeroes on screen would have the dashboard state
something false with no way for the reader to tell.

## The decoder is checked by a script, not by an eye

A manual pass over the vendored EDL decoder cross-checked all 195 channels
against Ecumaster's format definition and fixed 22 of them. It also missed
three, and the way it missed them is instructive: `cam1AngleTarget` and
`cam2AngleTarget` were both caught, and `cam1Angle` and `cam2Angle` sitting
directly above them were not. The eye read the group as done.

`tools/edl_check_storage.py` now re-derives every channel's storage and
divider from the XML and compares them against what `parseFrame()` does. It
reuses the same parser `emulog_inspect.py` already had, so there is one
definition of what a byte means, and it exits non-zero so it can gate a build.

The third miss, `idleAngleCorr`, is the reason this matters rather than a tidy
story: idle ignition correction is negative whenever the ECU pulls timing to
hold the target, so the channel was wrong at precisely the moment the new IDLE
page exists to show it.

## The logs go to the laptop, not to a cloud

The first plan for getting logs off the card without carrying it indoors was an
upload to Google Drive over the home WiFi. It was worked out in some detail -
OAuth with a refresh token in flash, TLS with pinned Google roots, resumable
uploads in 256 kB chunks, NTP so the certificate dates would validate - before
one fact about the workflow undid all of it.

The logs are read in EMU Classic Client, which runs on the Windows tuning
laptop. That laptop comes to the car anyway. So the cloud route was car → Drive
→ download back onto that same laptop: strictly longer than car → laptop, and
paid for with OAuth, certificate maintenance and a long-lived secret in a
firmware image anyone can dump.

The laptop's wireless is dedicated to this, so the network is ours to choose.
The dashboard raises its own access point and serves a page. What went away:
OAuth, TLS, root certificates, token expiry, the cloud quota, compile-time WiFi
credentials, and NTP - the browser's own clock sets the RTC, which needs no
internet at all. What was gained: it works in a garage, at a track, anywhere.

## The access point only runs with the engine stopped

Not a safety rule, though it reads like one. It is what keeps the feature small.

With the engine off the ECU sends nothing, so nothing is written to the card
while the server reads from it. No second task, no mutex, no SD card shared
between two cores - the server runs in the same cooperative loop as everything
else. The one failure this feature could plausibly have caused, a corrupted log,
is designed out rather than guarded against.

The cost is real and worth naming: a live view of all 195 channels would be the
most useful thing on that page when chasing a decoder bug, and it is impossible
here, because with the engine stopped there is nothing to show.

Guards exist because the alternator is not charging while this runs: a settle
delay, a voltage floor, and a fifteen-minute idle timeout that latches until the
engine runs again. See [service-page.md](service-page.md).

## Deleting logs from the page, after deciding not to

An earlier draft left deletion out, reasoning that a 32 GB card holds years of
logs at 4.7 MB per hour of driving, so nothing would ever need removing.

That argued about space when the question was about handling. Clearing the card
without deletion means taking it out and carrying it to a reader, which is the
exact chore the page exists to remove. It is in, with a confirmation that names
the files, no "delete all" form on the server, and a refusal to touch the file
currently being written.

---

## The battery and fuel-pressure alarms start switched off

Every alarm shipped enabled. Two of them should not.

The battery alarm trips on a charging system that dips under load, which is
most of them, and tells the driver something they already know. The fuel
pressure alarm has a worse problem: which Analog In carries fuel pressure is
still unconfirmed on this car, so the threshold may be watching a channel that
measures something else entirely.

An alarm that cries wolf does not cost nothing. It costs attention that the
real ones need, and it teaches the driver to look away from the status bar.
Both stay in the code, keep their thresholds, and are one tap away on the
setup page — they just do not arrive armed.

The settings version went to 3 to make this reach a device that already has a
stored record. Nothing about the record's shape changed; the bump only forces
the old one to be dropped. It takes brightness and the shift points with it,
which have to be set once more after the update.

---

## The alarm list scrolls, and the page gets first refusal on a swipe

The list holds 32 events and eleven fit on the screen, so a busy run put the
oldest 21 somewhere the driver could not reach.

Up and down now scroll it, a screen at a time with one row of overlap. The
mechanism is a virtual on `DashPage`: a swipe is offered to the page on screen
before it turns the page, and the page says whether it used it. Only the alarm
list takes anything, and only up and down — left and right stay reserved for
navigation everywhere, because a page that can trap the driver on itself is a
page that gets sworn at in traffic. A scroll that has hit the end reports the
swipe as unused rather than swallowing it.

A new event resets the scroll to the top. The newest is the one worth reading,
and it is the one that just happened.

---

## The palette is true greys now, because the panel is not neutral

The pass that darkened the value tiles left them at colours like `0x0D1113` —
green and blue a few counts above red. On paper that is a neutral so close to
black that the tint should not exist. On the panel in the car it read as a
green cast across every number on the screen.

So every grey in the theme is now R = G = B, and the tile background is plain
black. Where a tinted grey was replaced the luminance is unchanged, so the
only difference is hue. The warning and critical fills stay coloured; they are
supposed to be.

The lesson is about the display rather than the design: a 3.5" IPS driven by
an ILI9488 is not neutral enough to be trusted with a tint nobody asked for.
If a colour is meant to read as black, make it black.

---

## The log format is the EDL-1 frame, minus four bytes

The `.emulog` file EMU Classic Client writes turned out to be a gzip stream
over a 12-byte header and 256-byte records, where each record is the 260-byte
EDL-1 frame with its `32 40 50 60` marker stripped. Nothing is assembled,
scaled or reordered on the way in.

This collapsed the logging task. The plan had been to synthesise a record
format and reverse-engineer a field map; instead the map was already in the
vendored decoder, and the file wants the bytes the UART is already delivering.
The right design is therefore to tap the frame *before* decoding, not to build
records out of decoded values — a decode bug can never corrupt a log that never
went through the decoder.

It also settles the EDL-1 choice made earlier for a different reason. Adopting
EDL-1 was argued on channel count and simultaneous sampling; that it puts the
exact bytes the log format wants on the wire was a guess at the time. It is now
a fact.

Established from Antti's own logs and CSV exports, then proven by writing a
file that opens in the Client. Full account in
[emu-log-format.md](emu-log-format.md).

---

## Ecumaster's XML outranks the vendored decoder

`lib/EMUSerial-master/` is vendored unmodified, on the principle that the
reference implementation is the specification available. `lib/EDLSerial/` is
not, and the difference is that for EDL-1 a better specification exists:
`docs/ecu-formats/version1_211.xml` is Ecumaster's own format definition, and
its `storage` attribute states every channel's width and signedness.

Checking all 195 fields against it found 22 the library decodes wrongly —
mostly signed values read unsigned, so a small negative reading appears as a
large positive one. Three were provably wrong in real logs: `wboIPMeas` and
`wboIPNorm` in 83 % of samples, `dwellError` in 23 %. `IgnAngle`, which the
dashboard displays, is among the latent ones: correct all through both sample
logs and wrong the moment the ECU pulls timing.

The fixes went into the library rather than a wrapper, because unlike the
framing defects there is nothing to wrap — the information is destroyed at the
point of the cast. Upstream's README invites editing the parser, and the
changes are listed in [`lib/EDLSerial/README.md`](../lib/EDLSerial/README.md)
so an upstream pull cannot quietly revert them.

One field was deliberately left alone. `fcProbability` divides by 2 in the
library and by 1 in the XML; upstream targeted firmware 1.226 against this
project's 1.211, the channel is zero in every log available, and neither
reading could be confirmed. An unverified change is not an improvement.

---

## The alarm list is a log, not an operator's queue

The list was first drawn with an acknowledge column and an ACK ALL button, the
way an industrial alarm list works. That was dropped before any of it was
written.

In a control room acknowledgement is part of a chain of responsibility: someone
is on shift, sees the alarm, and takes it. In a car the person in front of the
screen is driving. Nobody is standing by to act on a row, and an unacknowledged
alarm does not mean anyone has failed to respond to it - it means the driver was
busy driving, which is what they are supposed to be doing. A button whose only
effect is to dim some text is a button that takes attention off the road.

What remains carries the same information in two channels that cost nothing to
read. Brightness answers "is this true right now": active rows are bright,
returned rows are dim. Colour answers "how bad was it", and the severity stripe
keeps its full colour on returned rows too, so the history stays scannable
instead of going flat grey.

Nothing blinks either. Blinking is standard practice for an unacknowledged
alarm and wrong here for the same reason: it is movement in the driver's
peripheral vision.

## Alarms are recorded as events, not one slot per alarm type

`AlarmEngine` latches trips into `latched_[kAlarmCount]` - one slot per alarm
id, holding the worst moment. That answers "did this ever trip", which is what
the Diag page asks, and it cannot answer "what happened, and in what order".
A coolant warning at 14:33 and another at 14:41 are the same slot, and the
second one is invisible.

`EventLog` is a ring of the most recent events, newest first, each stamped with
the wall clock, the rpm at onset and how long the condition held. An event that
is already open escalates rather than duplicating, so a warning that becomes
critical stays one event.

Three sources feed it, and the two new ones are the point of the exercise. The
ECU's check-engine word was previously only ever shown as the bits set right
now, so a fault that came and went while driving left no trace at all. Link
health was a counter, and a loose connector produces clusters - which are
visible in a time-ordered list and invisible in a count.

The old latched list stays on the Diag page for now. It is redundant against
this and removing it is a UI change of its own.

## The clock is set from the build, and only when it cannot be right

There is no network and no GPS in the car, so the only time the firmware can
know is the moment it was compiled. `__DATE__` and `__TIME__` are written to
the RTC, and a Clock category on the Setup page was considered and dropped: the
clock is the RTC's state, not a setting, and it does not belong in the settings
blob.

The interesting part is when to write it. The first attempt used the RTC's VL
flag alone, which was wrong. VL says the oscillator stopped. It does not say
the time is right - a part kept alive by its backup cell since the factory has
a clock that has been running the whole time and has never been set, VL clear
throughout. That is exactly what this board had: the panel came up reading
00:24 while the firmware reading it had been compiled at 19:44.

The second trigger is plausibility. A clock cannot legitimately read earlier
than the moment the firmware reading it was compiled, so anything that does has
never been set. Once seeded the RTC runs ahead of the build time and the test
stops firing; flashing a newer build over a correct clock leaves it alone; and
a clock that has drifted slow gets nudged forward by every reflash, which is a
useful accident rather than a designed feature.

The cost is daylight saving. The seed carries whatever offset was in force when
the file was compiled, so twice a year the clock is an hour out and the fix is
a reflash. That was accepted deliberately rather than build a settings UI for
it.

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
- **160 EDL-1 channels are decoded but not displayed.** Listed in
  [edl-channels.md](edl-channels.md); boost control, knock-versus-noise,
  trigger health and idle control are the ones this car can actually use.
- **The SD card mounts intermittently, and nobody knows why.** Measured on the
  bench: same card, same build, four consecutive resets gave mount, fail,
  mount, fail. Dropping the bus clock from an out-of-spec 80 MHz to 25 MHz
  changed nothing. A later session of eleven consecutive resets all mounted on
  the first attempt with nothing in the firmware to explain the difference -
  most likely the card seating itself, but that is a guess. `EmuLog::begin`
  now retries three times 100 ms apart and reports which attempt worked, so a
  `card mounted on attempt 2` in the console is the signal that the fault is
  back. Vibration in the car is its own test of that contact.
- **The Diag page's latched block is redundant.** The Alarms page supersedes
  it and does it properly. Removing the block frees about 60 px of the left
  column, which the link history would use well.
- **The clock drifts and does not know about daylight saving.** The crystal is
  uncompensated, so expect one to three minutes a month, always slow. Every
  reflash resyncs it; twice a year it is an hour out until one happens.
- **UI repaints at frame rate.** `DashUi` updates the visible page whenever the
  model's revision changes, which at 19200 baud is a few hundred times a
  second. LVGL coalesces the redraw, but the formatting work is done every
  time. Rate-limiting to about 20 Hz would cut it by an order of magnitude and
  make the digits readable rather than a blur.
- **LVGL object pool headroom: closed, the hard way, on 2026-09-19.** This
  entry used to say the pool was "unmeasured, but no longer suspect", and that
  if a page ever came up blank or the dash restarted on the way to one, this
  was the first thing to raise. It restarted on the way to the Limits
  category. `LV_MEM_SIZE` is 128 kB now, SETUP works from a fixed pool of rows
  so it no longer scales with the settings table, and the number is finally
  readable - the service page prints it under Diagnostics. See
  docs/test-results.md.
- **Enclosure dimensions are not verified.** `enclosure/case.scad` is
  parametric and its geometry is right, but the measurements at the top of the
  file are placeholders. They must be taken from Elecrow's STEP model or the
  physical panel before printing.
- **Fonts.** LVGL's built-in Montserrat is not condensed and stops at 48 px,
  so the RPM readout is smaller than the mockup's. `λ` and `Δ` are outside its
  character set, so the UI uses `LAMBDA`, `DFPR` and `C` instead of `°C`. A
  custom font subset would fix all of this at once - and would give the
  three-across pages more than the 36 px they read at now, since a condensed
  face at 44 px would fit the same 144 px cell.
- **Alarm state freezes when the link drops.** Evaluation now runs on a link
  state change as well as on new data - it has to, or a lost link would never
  be recorded as an event - but it re-evaluates the same stale snapshot, so
  the last known severities persist while the status bar reads `OFFLINE`.
  Deliberate for now; clearing them on `Offline` is a small change in
  `DashUi::update`.
