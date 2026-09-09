# Test results

Newest first. Record what was actually observed, not what was expected — an
entry that only says "worked" is worth very little later.

---

## 2026-09-09 — clock, alarm list and SD logging on the car

Firmware: `0e2ef5c`, environment `crowpanel_advance_35_edl`. First run of the
RTC, the Alarms page and SD logging on the hardware.

**Confirmed with the ECU connected**

- Values read correctly on screen and behave sensibly on all six pages.
- **The alarm list works on EDL-1.** Events land in the order they happened,
  with times, and the page reads the way it was designed to.
- This covers three of the items the 8 September entry listed as untested:
  alarm behaviour on a real engine, the run summary, and SD logging.

**The RTC seeding rule was wrong the first time**

The panel came up reading 00:24 while the firmware reading it had been compiled
at 19:44, so no seed had been written. The condition was the chip's VL flag
alone, and VL only says the oscillator stopped — it says nothing about whether
the time is right. With the backup cell fitted since the factory the clock had
been running the whole time and had never been set, VL clear throughout.

Fixed by adding a plausibility test: a clock reading earlier than the build it
is being read by has never been set. Verified on the next boot — the clock came
up correct and, on subsequent boots, was left alone rather than reseeded.

**SD logging: works, but mounted intermittently and the cause is unknown**

- With no card, `sdCommand(): Card Failed! cmd: 0x00` at CMD0, as expected.
- With a card, the same build over four consecutive resets: mount, fail,
  mount, fail. Card, wiring and filesystem are constant across those four, so
  none of them is the variable.
- Lowering the SPI clock from 80 MHz to 25 MHz **did not change it**. That was
  still worth doing — 80 MHz is outside what SD in SPI mode supports — but it
  was a fix for a different problem.
- A later session of eleven consecutive resets all mounted, every one on the
  **first** attempt, so the retry added in between never ran and cannot be
  credited. Something outside the firmware changed; card seating is the guess.
  Recorded as an open item rather than a closed one.

The file-number sequence is what proves the eleven: visible banners showed
00009, 00010, 00014, 00015, 00016 and 00019, and the gaps line up exactly with
the boots whose banner was lost, so those mounted too.

**Two smaller findings**

- **The console showed nothing after an upload.** All boot output is printed
  inside the first second, and a monitor attached afterwards misses it. The
  announce-on-connect added for this was not enough on its own: `Serial` reads
  as connected on this USB-CDC port before the host has re-opened it, so on
  about half the resets the reprint also went nowhere. Pressing any key now
  reprints the state, which does not depend on winning a race.
- **Stored settings were discarded.** `getBytes(): not enough space in buffer:
  132 < 136` — the record in NVS was written by a firmware whose `DashSettings`
  was four bytes larger. The size and version check rejected it and fell back
  to defaults, which is the designed behaviour. It stops once any setting is
  saved.

**Still untested**

- Whether the bad-frame count stays at zero while the card is being written —
  a long write during logging is the case that would push it.
- Whether the SD mount survives vibration in the car.
- Whether settings survive a power cycle (the stale record above has to be
  overwritten once before this can be checked at all).
- Backlight PWM, brightness and night mode.
- Whether the log file opens in EMU Classic Client. It does not, and the
  format question is still open.

---

## 2026-09-08 — EDL-1 link, after the framing fix

Firmware: `4c0b53b`, environment `crowpanel_advance_35_edl`. ECU reconfigured
to stream the EDL-1 logger protocol.

**Confirmed**

- The EDL-1 stream decodes correctly. Values read right on screen.
- **Bad frames stays at zero.** The 2048-byte receive buffer, the next-marker
  check and the plausibility gate together hold the link clean under normal
  running.
- Communication described as faultless over the session.

**What this replaces**

The first EDL-1 attempt showed bursts of impossible values across several
fields at once — CLT max 27472 C, oil pressure 15.9 bar, invented CEL flags.
Root cause was the 256-byte default UART buffer being smaller than one
260-byte frame. See the decision log.

**Still untested on EDL-1**

- Alarm behaviour across a real start, hysteresis, latching, run summary.
- Settings surviving a power cycle.
- Anything to do with SD logging.
- Whether the bad-frame count stays at zero under load — a long card write or
  a settings save is the case that would push it.

---

## 2026-09-08 — first run connected to the ECU

Firmware: `2bcd7a8` (five pages, settings in flash) on the CrowPanel Advance
3.5, ECU link through the MAX3232 on the EMU Classic extension port.

**Confirmed**

- Flashing over USB-C succeeds.
- The serial link comes up with the ECU connected.
- The user interface behaves logically — pages and navigation do what they
  are supposed to.
- Displayed values look correct as far as they have been checked against the
  ECU.

**Not tested yet**

- SD card logging. It is on the `feature/sd-logging` branch, which was not
  flashed.
- Alarm behaviour with a real engine running: the arming delay across a
  start, hysteresis on a value sitting at its limit, latching, and the
  engine-off run summary. All of it is untested against a real start.
- The Setup page under load: whether the LVGL object pool holds the Limits
  category, and whether vertical scrolling fights the horizontal page swipe.
- Backlight PWM, brightness and night mode.
- Whether settings actually survive a power cycle.

**Nothing to fix from this run.** No defects were observed.

### Worth checking on the next run

The two things most likely to be wrong are both alarm-related, because they
have never seen a real engine: whether the arming delay is long enough for oil
pressure to build on this engine, and whether the invented thresholds are
anywhere near right. Both are adjustable on the Setup page now, so the next
drive is the place to find out rather than a reflash.

---

## 2026-09-08 (later) — findings from the first run

Observed on the car and fixed in the same session:

- **The engine-off summary never appeared.** Cutting the ignition kills the ECU
  before RPM falls, so the last frame freezes at a few hundred rpm and the
  summary's trigger never fired. It now also ends the run on link loss, and is
  checked every pass rather than only when a frame arrives.
- **The idle return to Drive was unwanted.** It moved the screen away while a
  page was still being read. Removed; pages change only by swipe.
- **Three seconds of arming delay was too short.** Raised to six.
- **The boot sweep was over before you could look at it.** 900 ms to 2600 ms.
- **Cells looked washed out.** Panels darkened and the dividing lines lifted, so
  the numerals read against near-black.

Defaults corrected from what the car actually does: fuel pressure warn 3.2 to
2.2 bar and critical 2.6 to 1.9 bar, shift first light 3 500 to 1 000 rpm, red
zone 6 800 to 6 000.

Note that the settings blob version was bumped, so **the first boot after this
update discards any settings stored on the device** and comes up on the new
defaults.
